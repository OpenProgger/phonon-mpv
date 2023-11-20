/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2010 Ben Cooksley <sourtooth@gmail.com>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>
    Copyright (C) 2010-2015 Harald Sitter <sitter@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "mediaobject.h"

#include <QDir>
#include <QStringBuilder>
#include <QUrl>
#include <QList>

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/client.h>

#include "utils/debug.h"
#include "backend.h"
#include "sinknode.h"

//Time in milliseconds before sending aboutToFinish() signal
//2 seconds
static const int ABOUT_TO_FINISH_TIME = 2000;

using namespace Phonon::MPV;

static QList<void*> recentlyDestroyed;

MediaObject::MediaObject(QObject* parent)
    : QObject(parent)
    , m_nextSource(MediaSource(QUrl()))
    , m_state(Phonon::StoppedState)
    , m_tickInterval(0)
    , m_transitionTime(0) {

    if(!(m_player = mpv_create_client(Backend::self->handle(), nullptr))) {
        fatal() << "Failed to create MPV Client";
        return;
    }

    if(qgetenv("PHONON_BACKEND_DEBUG").toInt() >= 3) // 3 is maximum
        mpv_request_log_messages(m_player, "v");
    mpv_observe_property(m_player, 0, "time-pos", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_player, 1, "seekable", MPV_FORMAT_FLAG);
    mpv_observe_property(m_player, 2, "duration", MPV_FORMAT_DOUBLE);
    mpv_observe_property(m_player, 3, "paused-for-cache", MPV_FORMAT_FLAG);
    mpv_observe_property(m_player, 5, "pause", MPV_FORMAT_FLAG);
    mpv_observe_property(m_player, 7, "current-vo", MPV_FORMAT_STRING);
    mpv_observe_property(m_player, 8, "metadata", MPV_FORMAT_NODE);
    mpv_observe_property(m_player, 9, "mute", MPV_FORMAT_FLAG);
    mpv_observe_property(m_player, 10, "volume", MPV_FORMAT_INT64);
    mpv_set_wakeup_callback(m_player, MediaObject::event_cb, this);

    // Internal Signals.
    connect(this, SIGNAL(moveToNext()), SLOT(moveToNextSource()));
    connect(m_refreshTimer, SIGNAL(timeout()), this, SLOT(refreshDescriptors()));

    resetMembers();
}

MediaObject::~MediaObject() {
    recentlyDestroyed.append(this);
}

void MediaObject::event_cb(void *opaque) {
    if(recentlyDestroyed.contains(opaque)) {
        recentlyDestroyed.removeAll(opaque);
        return;
    }
    MediaObject* that = reinterpret_cast<MediaObject*>(opaque);
    QMetaObject::invokeMethod(
                    that,
                    "mpv_event_loop",
                    Qt::QueuedConnection);
}


void MediaObject::resetMembers() {
    DEBUG_BLOCK;
    // default to -1, so that streams won't break and to comply with the docs (-1 if unknown)
    m_totalTime = -1;
    m_hasVideo = false;
    m_seekpoint = 0;
    m_prefinishEmitted = false;
    m_aboutToFinishEmitted = false;
    m_lastTick = 0;
    m_buffering = false;
    m_stateAfterBuffering = ErrorState;
    resetMediaController();
}

void MediaObject::play() {
    DEBUG_BLOCK;
    if(m_state == PausedState) {
        auto err{0};
        auto play{0};
        if((err = mpv_set_property(m_player, "pause", MPV_FORMAT_FLAG, &play)))
            error() << "Failed to play file" << mpv_error_string(err);
    }
}

void MediaObject::pause() {
    DEBUG_BLOCK;
    if(m_state == BufferingState || m_state == PlayingState) {
        auto err{0};
        auto pause{1};
        if((err = mpv_set_property(m_player, "pause", MPV_FORMAT_FLAG, &pause)))
            error() << "Failed to pause file" << mpv_error_string(err);
    }
}

void MediaObject::stop() {
    DEBUG_BLOCK;
    m_nextSource = MediaSource(QUrl());
    auto err{0};
    const char* cmd[] = {"stop", nullptr};
    if((err = mpv_command(m_player, cmd)))
        error() << "Failed to stop media:" << mpv_error_string(err);
    updateState(StoppedState);
}

void MediaObject::seek(qint64 milliseconds) {
    DEBUG_BLOCK;

    if(m_state != PlayingState && m_state != PausedState && m_state != BufferingState) {
        m_seekpoint = milliseconds;
        return;
    }

    debug() << "seeking" << milliseconds << "msec";

    auto err{0};
    double nowTime{milliseconds / 1000.0f};
    if((err = mpv_set_property(m_player, "time-pos", MPV_FORMAT_DOUBLE, &nowTime)))
        error() << "Failed to set time:" << mpv_error_string(err);

    const qint64 time = currentTime();
    const qint64 total = totalTime();

    // Reset last tick marker so we emit time even after seeking
    if(time < m_lastTick)
        m_lastTick = time;
    if(time < total - m_prefinishMark)
        m_prefinishEmitted = false;
    if(time < total - ABOUT_TO_FINISH_TIME)
        m_aboutToFinishEmitted = false;
}

void MediaObject::timeChanged(qint64 time) {
    const qint64 totalTime = m_totalTime;

    if(m_state == PlayingState || m_state == BufferingState || m_state == PausedState)
        emitTick(time);

    if(m_state == PlayingState || m_state == BufferingState) { // Buffering is concurrent
        if (time >= totalTime - m_prefinishMark) {
            if (!m_prefinishEmitted) {
                m_prefinishEmitted = true;
                emit prefinishMarkReached(totalTime - time);
            }
        }
        // Note that when the totalTime is <= 0 we cannot calculate any sane delta.
        if (totalTime > 0 && time >= totalTime - ABOUT_TO_FINISH_TIME)
            emitAboutToFinish();
    }
}

void MediaObject::emitTick(qint64 time) {
    if (m_tickInterval == 0) // Make sure we do not ever emit ticks when deactivated.\]
        return;
    if (time + m_tickInterval >= m_lastTick) {
        m_lastTick = time;
        emit tick(time);
    }
}

void MediaObject::loadMedia(const QString &mrl) {
    DEBUG_BLOCK;

    emit hasVideoChanged(true);

    debug() << "loading encoded:" << m_mrl;
    if(mrl.length())
        m_mrl = mrl.toUtf8();
    resetMembers();
    auto err{0};
    if(m_state == PlayingState)
        updateState(StoppedState);
    const char* cmd[]{"loadfile", m_mrl.constData(), nullptr};
    debug() << "Play File " << m_mrl;
    if((err = mpv_command(m_player, cmd)))
        error() << "Failed to load media:" << mpv_error_string(err);
}

qint32 MediaObject::tickInterval() const {
    return m_tickInterval;
}

/**
 * Supports runtime changes.
 * If the user goes to tick(0) we stop the timer, otherwise we fire it up.
 */
void MediaObject::setTickInterval(qint32 interval) {
    m_tickInterval = interval;
}

qint64 MediaObject::currentTime() const {
    qint64 time = -1;

    switch (m_state) {
        case PausedState:
        case BufferingState:
        case PlayingState: {
            auto err{0};
            double duration{0.0f};
            if((err = mpv_get_property(m_player, "time-pos", MPV_FORMAT_DOUBLE, &duration)))
                warning() << "Failed to get time:" << mpv_error_string(err);
            time = duration * 1000;
        }
        break;
        case StoppedState:
        case LoadingState:
            time = 0;
            break;
        case ErrorState:
            time = -1;
            break;
    }

    return time;
}

Phonon::State MediaObject::state() const {
    DEBUG_BLOCK;
    return m_state;
}

Phonon::ErrorType MediaObject::errorType() const {
    DEBUG_BLOCK;
    return Phonon::NormalError;
}

Phonon::MediaSource MediaObject::source() const {
    DEBUG_BLOCK;
    return m_mediaSource;
}

void MediaObject::setSource(const MediaSource &source) {
    DEBUG_BLOCK;

    m_mediaSource = source;
    QByteArray url;
    switch(source.type()) {
        case MediaSource::Invalid:
            error() << Q_FUNC_INFO << "MediaSource Type is Invalid:" << source.type();
            break;
        case MediaSource::Empty:
            error() << Q_FUNC_INFO << "MediaSource is empty.";
            break;
        case MediaSource::LocalFile:
        case MediaSource::Url:
            debug() << "MediaSource::Url:" << source.url();
            if(source.url().scheme().isEmpty()) {
                url = "file://";
                // QUrl considers url.scheme.isEmpty() == url.isRelative(),
                // so to be sure the url is not actually absolute we just
                // check the first character
                if (!source.url().toString().startsWith('/'))
                    url.append(QFile::encodeName(QDir::currentPath()) + '/');
            }
            url += source.url().toEncoded();
            loadMedia(url);
            break;
        case MediaSource::Disc:
            switch (source.discType()) {
                case Phonon::NoDisc:
                    error() << Q_FUNC_INFO << "the MediaSource::Disc doesn't specify which one (Phonon::NoDisc)";
                    return;
                case Phonon::Cd:
                    loadMedia(QLatin1Literal("cdda://") % m_mediaSource.deviceName());
                    break;
                case Phonon::Dvd:
                    loadMedia(QLatin1Literal("dvd://") % m_mediaSource.deviceName());
                    break;
                case Phonon::Vcd:
                    loadMedia(QLatin1Literal("vcd://") % m_mediaSource.deviceName());
                    break;
                case Phonon::BluRay:
                    loadMedia(QLatin1Literal("bluray://") % m_mediaSource.deviceName());
                    break;
            }
            break;
        case MediaSource::CaptureDevice: {
            QByteArray driverName;
            QString deviceName;

            if(source.deviceAccessList().isEmpty()) {
                error() << Q_FUNC_INFO << "No device access list for this capture device";
                break;
            }

            // TODO try every device in the access list until it works, not just the first one
            driverName = source.deviceAccessList().first().first;
            deviceName = source.deviceAccessList().first().second;

            if (driverName == QByteArray("v4l2"))
                loadMedia(QLatin1Literal("v4l2://") % deviceName);
            else if (driverName == QByteArray("alsa"))
                loadMedia(QLatin1Literal("alsa://") % deviceName);
            else if (driverName == "screen")
                loadMedia(QLatin1Literal("screen://") % deviceName);
            else
                error() << Q_FUNC_INFO << "Unsupported MediaSource::CaptureDevice:" << driverName;
        }
        break;
        case MediaSource::Stream:
        default:
            break;
    }

    debug() << "Sending currentSourceChanged";
    emit currentSourceChanged(m_mediaSource);
}

void MediaObject::setNextSource(const MediaSource &source) {
    DEBUG_BLOCK;
    debug() << source.url();
    m_nextSource = source;
    // This function is not ever called by the consumer but only libphonon.
    // Furthermore libphonon only calls this function in its aboutToFinish slot,
    // iff sources are already in the queue. In case our aboutToFinish was too
    // late we may already be stopped when the slot gets activated.
    // Therefore we need to make sure that we move to the next source iff
    // this function is called when we are in stoppedstate.
    if (m_state == StoppedState)
        moveToNext();
}

qint32 MediaObject::prefinishMark() const {
    return m_prefinishMark;
}

void MediaObject::setPrefinishMark(qint32 msecToEnd) {
    m_prefinishMark = msecToEnd;
    if (currentTime() < totalTime() - m_prefinishMark)
        m_prefinishEmitted = false;
}

qint32 MediaObject::transitionTime() const {
    return m_transitionTime;
}

void MediaObject::setTransitionTime(qint32 time) {
    m_transitionTime = time;
}

void MediaObject::emitAboutToFinish() {
    DEBUG_BLOCK;
    if (!m_aboutToFinishEmitted) {
        // Track is about to finish
        m_aboutToFinishEmitted = true;
        emit aboutToFinish();
    }
}

// State changes are force queued by libphonon.
void MediaObject::changeState(Phonon::State newState) {
    DEBUG_BLOCK;

    // State not changed
    if (newState == m_state)
        return;

    debug() << m_state << "-->" << newState;

    // Workaround that seeking needs to work before the file is being played...
    // We store seeks and apply them when going to seek (or discard them on reset).
    if (newState == PlayingState) {
        if (m_seekpoint != 0) {
            seek(m_seekpoint);
            m_seekpoint = 0;
        }
    }

    // State changed
    Phonon::State previousState = m_state;
    m_state = newState;
    emit stateChanged(m_state, previousState);
}

void MediaObject::moveToNextSource() {
    DEBUG_BLOCK;
    setSource(m_nextSource);
    m_nextSource = MediaSource(QUrl());
}

QString MediaObject::errorString() const {
    DEBUG_BLOCK;
    return mpv_error_string(0);
}

bool MediaObject::hasVideo() const {
    DEBUG_BLOCK
    if(m_mrl.isEmpty())
        return false;
    return mpv_get_property_string(m_player, "video-format");
}

bool MediaObject::isSeekable() const {
    DEBUG_BLOCK;
    auto seekable{0};
    if(mpv_get_property(m_player, "seekable", MPV_FORMAT_FLAG, &seekable))
        return false;
    return seekable;
}

void MediaObject::updateMetaData() {
    DEBUG_BLOCK;
    QMultiMap<QString, QString> metaDataMap;

    auto err{0};
    mpv_node metaData;
    if((err = mpv_get_property(m_player, "metadata", MPV_FORMAT_NODE, &metaData)))
        warning() << "Failed to get title count:" << mpv_error_string(err);

    for(auto i{0}; i < metaData.u.list->num; i++) {
        QString key(metaData.u.list->keys[i]);
        if(key == "title")
            metaDataMap.insert(QLatin1String("TITLE"), metaData.u.list->values[i].u.string);
        else if(key == "artist")
            metaDataMap.insert(QLatin1String("ARTIST"), metaData.u.list->values[i].u.string);
        else if(key == "date")
            metaDataMap.insert(QLatin1String("DATE"), metaData.u.list->values[i].u.string);
        else if(key == "genre")
            metaDataMap.insert(QLatin1String("GENRE"), metaData.u.list->values[i].u.string);
        else if(key == "encoder")
            metaDataMap.insert(QLatin1String("ENCODEDBY"), metaData.u.list->values[i].u.string);
        else
            metaDataMap.insert(QLatin1String(metaData.u.list->keys[i]), metaData.u.list->values[i].u.string);
    }

    if(!metaDataMap.contains(QLatin1String("TITLE"))) {
        char* title{nullptr};
        if(!(title = mpv_get_property_string(m_player, "media-title")))
            warning() << "Failed to get title name";
        else {
            metaDataMap.insert(QLatin1String("TITLE"), title);
            mpv_free(title);
        }
    }
    int64_t track{0};
    if((err = mpv_get_property(m_player, "playlist-pos", MPV_FORMAT_INT64, &track)))
        warning() << "Failed to get track number";
    metaDataMap.insert(QLatin1String("TRACKNUMBER"), QString::number(track));
    metaDataMap.insert(QLatin1String("URL"), m_mrl);
    mpv_free_node_contents(&metaData);

    if(metaDataMap == m_mpvMetaData)
        return;
    m_mpvMetaData = metaDataMap;
    emit metaDataChanged(metaDataMap);
}

void MediaObject::updateState(Phonon::State state) {
    DEBUG_BLOCK;
    debug() << "attempted autoplay?" << m_attemptingAutoplay;

    if(m_attemptingAutoplay && (state == PlayingState || state == PausedState))
        m_attemptingAutoplay = false;

    if(state == ErrorState) {
        if(m_attemptingAutoplay)
            --m_currentTitle;
        emitAboutToFinish();
        emit finished();
    }
    changeState(state);
    if (m_buffering) {
        switch (state) {
            case BufferingState:
                break;
            case PlayingState:
                debug() << "Restoring buffering state after state change to Playing";
                changeState(BufferingState);
                m_stateAfterBuffering = PlayingState;
                break;
            case PausedState:
                debug() << "Restoring buffering state after state change to Paused";
                changeState(BufferingState);
                m_stateAfterBuffering = PausedState;
                break;
            default:
                debug() << "Buffering aborted!";
                m_buffering = false;
                break;
        }
    }
}

void MediaObject::onHasVideoChanged(bool hasVideo) {
    DEBUG_BLOCK;
    if(m_hasVideo != hasVideo) {
        m_hasVideo = hasVideo;
        emit hasVideoChanged(m_hasVideo);
        refreshDescriptors();
    } 
}

void MediaObject::refreshDescriptors() {
    DEBUG_BLOCK;
    auto err{0};
    int64_t count{0};
    if((err = mpv_get_property(m_player, "playlist-count", MPV_FORMAT_INT64, &count)))
        warning() << "Failed to get title count:" << mpv_error_string(err);
    if(count > 0)
        refreshTitles();

    if(hasVideo()) {
        refreshAudioChannels();
        refreshSubtitles();

        if((err = mpv_get_property(m_player, "chapters", MPV_FORMAT_INT64, &count)))
            warning() << "Failed to get video chapters:" << mpv_error_string(err);
        if(count > 0) {
            refreshChapters();
            refreshAngles();
        }
    }
}

void MediaObject::mpv_event_loop() {
    // Do not forget to register for the events you want to handle here!
    while(m_player) {
        mpv_event *event = mpv_wait_event(m_player, 0);
        //debug() << "Event " << event->event_id;
        switch (event->event_id) {
            case MPV_EVENT_LOG_MESSAGE: {
                QString msg("[");
                msg.append(((mpv_event_log_message*)event->data)->prefix);
                msg.append("]");
                msg.append(((mpv_event_log_message*)event->data)->text);
                switch(((mpv_event_log_message*)event->data)->log_level) {
                    case MPV_LOG_LEVEL_FATAL:
                        fatal() << msg;
                        break;
                    case MPV_LOG_LEVEL_ERROR:
                        error() << msg;
                        break;
                    case MPV_LOG_LEVEL_WARN:
                        warning() << msg;
                        break;
                    case MPV_LOG_LEVEL_INFO:
                    case MPV_LOG_LEVEL_V:
                        debug() << msg;
                        break;
                    default:
                        break;
                }
            }
            break;
            case MPV_EVENT_PROPERTY_CHANGE:
                //debug() << "Changed Property " << event->reply_userdata;
                switch(event->reply_userdata) {
                    case 0:
                        if(((mpv_event_property*)event->data)->format)
                            timeChanged(static_cast<qint64>(*(double*)((mpv_event_property*)event->data)->data * 1000));
                        break;
                    case 1:
                        if(((mpv_event_property*)event->data)->format)
                            seekableChanged(static_cast<bool>(*(int*)((mpv_event_property*)event->data)->data));
                        break;
                    case 2:
                        if(((mpv_event_property*)event->data)->format) {
                            m_totalTime = static_cast<qint64>(*(double*)((mpv_event_property*)event->data)->data * 1000);
                            emit totalTimeChanged(m_totalTime);
                        }
                        break;
                    case 3:
                        if(((mpv_event_property*)event->data)->format) {
                            if(*(int*)((mpv_event_property*)event->data)->data) {
                                m_buffering = true;
                                if(m_state != BufferingState) {
                                    m_stateAfterBuffering = m_state;
                                    changeState(BufferingState);
                                }
                                mpv_observe_property(m_player, 4, "cache-buffering-state", MPV_FORMAT_INT64);
                            } else if(m_buffering) {
                                m_buffering = false;
                                changeState(m_stateAfterBuffering);
                                mpv_unobserve_property(m_player, 4);
                            }
                        }
                        break;
                    case 4:
                        if(((mpv_event_property*)event->data)->format)
                            emit bufferStatus(static_cast<int>(*(int64_t*)((mpv_event_property*)event->data)->data));
                        break;
                    case 5:
                        if(((mpv_event_property*)event->data)->format) {
                            if(*(int*)((mpv_event_property*)event->data)->data)
                                updateState(PausedState);
                            else if(m_state != PlayingState)
                                updateState(PlayingState);
                        }
                        break;
                    case 7:
                        if(((mpv_event_property*)event->data)->format) {
                            if((char*)((mpv_event_property*)event->data)->data)
                                hasVideoChanged(true);
                            else
                                hasVideoChanged(false);
                        }
                        break;
                    case 8:
                        if(((mpv_event_property*)event->data)->format)
                            updateMetaData();
                        break;
                    case 9:
                        if(((mpv_event_property*)event->data)->format)
                            emit mutedChanged(*(int*)((mpv_event_property*)event->data)->data != 0);
                        break;
                    case 10:
                        if(((mpv_event_property*)event->data)->format)
                            emit volumeChanged(*(int64_t*)((mpv_event_property*)event->data)->data);
                        break;
                }
                break;
            case MPV_EVENT_START_FILE:
                updateState(LoadingState);
                break;
            case MPV_EVENT_FILE_LOADED:
                refreshDescriptors();
                updateState(PlayingState);
                break;
            case MPV_EVENT_COMMAND_REPLY: 
                if(event->error < 0)
                    updateState(ErrorState);
                break;
            case MPV_EVENT_END_FILE:
                if(m_state != StoppedState) {
                    if(m_nextSource.type() != MediaSource::Invalid && m_nextSource.type() != MediaSource::Empty) {
                        moveToNextSource();
                    } else if(source().discType() == Cd && m_autoPlayTitles && !m_attemptingAutoplay) {
                        debug() << "trying to simulate autoplay";
                        m_attemptingAutoplay = true;
                        int64_t id{++m_currentTitle};
                        auto err{0};
                        if((err = mpv_set_property(m_player, "vid", MPV_FORMAT_INT64, &id)))
                            warning() << "Failed to take CD track:" << mpv_error_string(err);
                    } else {
                        m_attemptingAutoplay = false;
                        emitAboutToFinish();
                        emit finished();
                        changeState(StoppedState);
                    }
                }
                break;
            default:
                break;
        }
        if(event->event_id == MPV_EVENT_NONE)
            break;
    }
}

qint64 MediaObject::totalTime() const {
    DEBUG_BLOCK;
    return m_totalTime;
}

void MediaObject::addSink(SinkNode* node) {
    DEBUG_BLOCK;
    Q_ASSERT(!m_sinks.contains(node));
    m_sinks.append(node);
}

void MediaObject::removeSink(SinkNode* node) {
    DEBUG_BLOCK;
    Q_ASSERT(node);
    m_sinks.removeAll(node);
}
