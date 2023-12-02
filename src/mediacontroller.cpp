/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>
    Copyright (C) 2011-2018 Harald Sitter <sitter@kde.org>

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

#include "mediacontroller.h"

#include <phonon/GlobalDescriptionContainer>

#include <QTimer>

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/client.h>

#include "utils/debug.h"

using namespace Phonon::MPV;

MediaController::MediaController()
    : m_subtitleAutodetect(true)
    , m_subtitleEncoding("UTF-8")
    , m_subtitleFontChanged(false)
    , m_player(nullptr)
    , m_refreshTimer(new QTimer(dynamic_cast<QObject *>(this)))
    , m_attemptingAutoplay(false) {
    GlobalSubtitles::instance()->register_(this);
    GlobalAudioChannels::instance()->register_(this);
    resetMembers();
}

MediaController::~MediaController() {
    GlobalSubtitles::instance()->unregister_(this);
    GlobalAudioChannels::instance()->unregister_(this);
}

bool MediaController::hasInterface(Interface iface) const {
    switch (iface) {
        case AddonInterface::NavigationInterface:
            return true;
            break;
        case AddonInterface::ChapterInterface:
            return true;
            break;
        case AddonInterface::AngleInterface:
            return true;
            break;
        case AddonInterface::TitleInterface:
            return true;
            break;
        case AddonInterface::SubtitleInterface:
            return true;
            break;
        case AddonInterface::AudioChannelInterface:
            return true;
            break;
    }

    warning() << "Interface" << iface << "is not supported by Phonon MPV :(";
    return false;
}

QVariant MediaController::interfaceCall(Interface iface, int i_command, const QList<QVariant>& arguments) {
    DEBUG_BLOCK;
    switch(iface) {
        case AddonInterface::ChapterInterface:
            switch (static_cast<AddonInterface::ChapterCommand>(i_command)) {
                case AddonInterface::availableChapters:
                    return availableChapters();
                case AddonInterface::chapter:
                    return currentChapter();
                case AddonInterface::setChapter:
                    if(arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                        error() << Q_FUNC_INFO << "arguments invalid";
                        return false;
                    }
                    setCurrentChapter(arguments.first().toInt());
                    return true;
            }
            break;
        case AddonInterface::TitleInterface:
            switch (static_cast<AddonInterface::TitleCommand>(i_command)) {
                case AddonInterface::availableTitles:
                    return availableTitles();
                case AddonInterface::title:
                    return currentTitle();
                case AddonInterface::setTitle:
                    if(arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                        error() << Q_FUNC_INFO << "arguments invalid";
                        return false;
                    }
                    setCurrentTitle(arguments.first().toInt());
                    return true;
                case AddonInterface::autoplayTitles:
                    return autoplayTitles();
                case AddonInterface::setAutoplayTitles:
                    if(arguments.isEmpty() || !arguments.first().canConvert(QVariant::Bool)) {
                        error() << Q_FUNC_INFO << " arguments invalid";
                        return false;
                    }
                    setAutoplayTitles(arguments.first().toBool());
                    return true;
            }
            break;
        case AddonInterface::AngleInterface:
            switch(static_cast<AddonInterface::AngleCommand>(i_command)) {
                case AddonInterface::availableAngles:
                    return availableAngles();
                case AddonInterface::angle:
                    return currentAngle();
                case AddonInterface::setAngle:
                    if(arguments.isEmpty() || !arguments.first().canConvert(QVariant::Int)) {
                        error() << Q_FUNC_INFO << "arguments invalid";
                        return false;
                    }
                    setCurrentAngle(arguments.first().toInt());
                    return true;
            }
            break;
        case AddonInterface::SubtitleInterface:
            switch(static_cast<AddonInterface::SubtitleCommand>(i_command)) {
                case AddonInterface::availableSubtitles:
                    return QVariant::fromValue(availableSubtitles());
                case AddonInterface::currentSubtitle:
                    return QVariant::fromValue(currentSubtitle());
                case AddonInterface::setCurrentSubtitle:
                    if(arguments.isEmpty() || !arguments.first().canConvert<SubtitleDescription>()) {
                        error() << Q_FUNC_INFO << "arguments invalid";
                        return false;
                    }
                    setCurrentSubtitle(arguments.first().value<SubtitleDescription>());
                    return true;
                case AddonInterface::setCurrentSubtitleFile:
                    if(arguments.isEmpty() || !arguments.first().canConvert<QUrl>()) {
                        error() << Q_FUNC_INFO << " arguments invalid";
                        return false;
                    }
                    setCurrentSubtitleFile(arguments.first().value<QUrl>());
                    [[fallthrough]];
                case AddonInterface::subtitleAutodetect:
                    return QVariant::fromValue(subtitleAutodetect());
                case AddonInterface::setSubtitleAutodetect:
                    if(arguments.isEmpty() || !arguments.first().canConvert<bool>()) {
                        error() << Q_FUNC_INFO << " arguments invalid";
                        return false;
                    }
                    setSubtitleAutodetect(arguments.first().value<bool>());
                    return true;
                case AddonInterface::subtitleEncoding:
                    return subtitleEncoding();
                case AddonInterface::setSubtitleEncoding:
                    if(arguments.isEmpty() || !arguments.first().canConvert<QString>()) {
                        error() << Q_FUNC_INFO << " arguments invalid";
                        return false;
                    }
                    setSubtitleEncoding(arguments.first().value<QString>());
                    return true;
                case AddonInterface::subtitleFont:
                    return subtitleFont();
                case AddonInterface::setSubtitleFont:
                    if(arguments.isEmpty() || !arguments.first().canConvert<QFont>()) {
                        error() << Q_FUNC_INFO << " arguments invalid";
                        return false;
                    }
                    setSubtitleFont(arguments.first().value<QFont>());
                    return true;
            }
            break;
        case AddonInterface::AudioChannelInterface:
            switch(static_cast<AddonInterface::AudioChannelCommand>(i_command)) {
                case AddonInterface::availableAudioChannels:
                    return QVariant::fromValue(availableAudioChannels());
                case AddonInterface::currentAudioChannel:
                    return QVariant::fromValue(currentAudioChannel());
                case AddonInterface::setCurrentAudioChannel:
                    if(arguments.isEmpty() || !arguments.first().canConvert<AudioChannelDescription>()) {
                        error() << Q_FUNC_INFO << "arguments invalid";
                        return false;
                    }
                    setCurrentAudioChannel(arguments.first().value<AudioChannelDescription>());
                    return true;
            }
            break;
        default:
            break;
    }

    error() << Q_FUNC_INFO << "unsupported AddonInterface::Interface:" << iface;

    return QVariant();
}

void MediaController::resetMediaController() {
    resetMembers();
    emit availableAudioChannelsChanged();
    emit availableSubtitlesChanged();
    emit availableTitlesChanged(0);
    emit availableChaptersChanged(0);
    emit availableAnglesChanged(0);
}

void MediaController::resetMembers() {
    m_currentAudioChannel = Phonon::AudioChannelDescription();
    GlobalAudioChannels::self->clearListFor(this);

    m_currentSubtitle = Phonon::SubtitleDescription();
    GlobalSubtitles::instance()->clearListFor(this);

    m_currentChapter = 0;
    m_availableChapters = 0;

    m_currentAngle = 0;
    m_availableAngles = 0;
    
    m_currentTitle = 1;
    m_availableTitles = 0;

    m_attemptingAutoplay = false;
}

// ----------------------------- Audio Channel ------------------------------ //
void MediaController::setCurrentAudioChannel(const Phonon::AudioChannelDescription& audioChannel) {
    int64_t localIndex = GlobalAudioChannels::instance()->localIdFor(this, audioChannel.index());
    auto err{0};
    if((err = mpv_set_property(m_player, "aid", MPV_FORMAT_INT64, &localIndex)))
        error() << "Failed to set Audio Track:" << mpv_error_string(err);
    else
        m_currentAudioChannel = audioChannel;
}

QList<Phonon::AudioChannelDescription> MediaController::availableAudioChannels() const {
    return GlobalAudioChannels::instance()->listFor(this);
}

Phonon::AudioChannelDescription MediaController::currentAudioChannel() const {
    return m_currentAudioChannel;
}

void MediaController::refreshAudioChannels() {
    GlobalAudioChannels::instance()->clearListFor(this);

    int64_t currentChannelId{0};
    auto err{0};
    if((err = mpv_get_property(m_player, "aid", MPV_FORMAT_INT64, &currentChannelId)))
        error() << "Failed to get Audio Track:" << mpv_error_string(err);

    mpv_node audioChannels;
    if((err = mpv_get_property(m_player, "track-list", MPV_FORMAT_NODE, &audioChannels)))
        error() << "Failed to get Audio Channels:" << mpv_error_string(err);
    for(auto i{0}; i < audioChannels.u.list->num; i++) {
        if(QString(audioChannels.u.list->values[i].u.list->values[1].u.string) == "audio") {
            auto id{0};
            QString title;
            for(auto j{0}; j < audioChannels.u.list->values[i].u.list->num; j++) {
                if(QString(audioChannels.u.list->values[i].u.list->keys[j]) == "id")
                    id = audioChannels.u.list->values[i].u.list->values[j].u.int64;
                if(QString(audioChannels.u.list->values[i].u.list->keys[j]) == "lang")
                    title = audioChannels.u.list->values[i].u.list->values[j].u.string;
            }
            GlobalAudioChannels::instance()->add(this, id, title.isEmpty() ? "Title " + QString::number(id) : title, "");
            if(i == currentChannelId) {
                const QList<AudioChannelDescription> list{GlobalAudioChannels::instance()->listFor(this)};
                foreach(const AudioChannelDescription &descriptor, list) {
                    if(descriptor.name() == QChar(id))
                        m_currentAudioChannel = descriptor;
                }
            }
        }
    }
    mpv_free_node_contents(&audioChannels);

    emit availableAudioChannelsChanged();
}

// -------------------------------- Subtitle -------------------------------- //
void MediaController::setCurrentSubtitle(const Phonon::SubtitleDescription &subtitle) {
    DEBUG_BLOCK;
    QString type = subtitle.property("type").toString();

    debug() << subtitle;

    auto err{0};
    if(type == "file") {
        QString filename{subtitle.property("name").toString()};
        if(!filename.isEmpty()) {
            const char* cmd[] = {"sub-add", filename.toUtf8().constData(), nullptr};
            if((err = mpv_command(m_player, cmd)))
                error() << "Failed to set Subtitle:" << mpv_error_string(err);
            else
                m_currentSubtitle = subtitle;

            GlobalSubtitles::instance()->add(this, m_currentSubtitle);
            emit availableSubtitlesChanged();
        }
    } else {
        int64_t localIndex{GlobalSubtitles::instance()->localIdFor(this, subtitle.index())};
        debug() << "localid" << localIndex;
        if((err = mpv_set_property(m_player, "sid", MPV_FORMAT_INT64, &localIndex)))
            error() << "Failed to set Subtitle:" << mpv_error_string(err);
        else
            m_currentSubtitle = subtitle;
    }
}

void MediaController::setCurrentSubtitleFile(const QUrl &url) {
    const QString file{url.toLocalFile()};
    auto err{0};
    const char* cmd[] = {"sub-add", file.toUtf8().constData(), nullptr};
    if((err = mpv_command(m_player, cmd)))
        error() << "Failed to set Subtitle File:" << mpv_error_string(err);
    // Unfortunately the addition of SPUs does not trigger an event in the
    // MPV mediaplayer, yet the actual addition to the descriptor is async.
    // So for the time being our best shot at getting an up-to-date list of SPUs
    // is shooting in the dark and hoping we hit something.
    // Refresha after 1, 2 and 5 seconds. If we have no updated list after 5
    // seconds we are out of luck.
    QObject* mediaObject{dynamic_cast<QObject*>(this)}; // MediaObject : QObject, MediaController
    m_refreshTimer->singleShot(1 * 1000, mediaObject, SLOT(refreshDescriptors()));
    m_refreshTimer->singleShot(2 * 1000, mediaObject, SLOT(refreshDescriptors()));
    m_refreshTimer->singleShot(5 * 1000, mediaObject, SLOT(refreshDescriptors()));
}

QList<Phonon::SubtitleDescription> MediaController::availableSubtitles() const {
    return GlobalSubtitles::instance()->listFor(this);
}

Phonon::SubtitleDescription MediaController::currentSubtitle() const {
    return m_currentSubtitle;
}

void MediaController::refreshSubtitles() {
    DEBUG_BLOCK;
    GlobalSubtitles::instance()->clearListFor(this);

    int64_t currentSubtitleId{0};
    auto err{0};
    if((err = mpv_get_property(m_player, "aid", MPV_FORMAT_INT64, &currentSubtitleId)))
        error() << "Failed to get Audio Track:" << mpv_error_string(err);

    mpv_node subtitles;
    if((err = mpv_get_property(m_player, "track-list", MPV_FORMAT_NODE, &subtitles)))
        error() << "Failed to get Subtitles:" << mpv_error_string(err);
    for(auto i{0}; i < subtitles.u.list->num; i++) {
        if(QString(subtitles.u.list->values[i].u.list->values[1].u.string) == "sub") {
            auto id{0};
            QString title;
            bool force{false};
            for(auto j{0}; j < subtitles.u.list->values[i].u.list->num; j++) {
                if(QString(subtitles.u.list->values[i].u.list->keys[j]) == "id")
                    id = subtitles.u.list->values[i].u.list->values[j].u.int64;
                if(QString(subtitles.u.list->values[i].u.list->keys[j]) == "lang")
                    title = subtitles.u.list->values[i].u.list->values[j].u.string;
                if(QString(subtitles.u.list->values[i].u.list->keys[j]) == "forced" && subtitles.u.list->values[i].u.list->values[j].u.flag)
                    force = true;
            }
            debug() << "found subtitle" << title << "[" << id << "]";
            GlobalSubtitles::instance()->add(this, id, (title.isEmpty() ? "Subtitle " + QString::number(id) : title) + (force ? "[FORCED]" : ""), "");
            if(id == currentSubtitleId) {
                const QList<SubtitleDescription> list{GlobalSubtitles::instance()->listFor(this)};
                foreach(const SubtitleDescription &descriptor, list) {
                    if(descriptor.name() == title)
                        m_currentSubtitle = descriptor;
                }
            }
        }
    }
    mpv_free_node_contents(&subtitles);

    emit availableSubtitlesChanged();
}

bool MediaController::subtitleAutodetect() const {
    return m_subtitleAutodetect;
}

void MediaController::setSubtitleAutodetect(bool enabled) {
    m_subtitleAutodetect = enabled;
}

QString MediaController::subtitleEncoding() const {
    return m_subtitleEncoding;
}

void MediaController::setSubtitleEncoding(const QString& encoding) {
    m_subtitleEncoding = encoding;
}

QFont MediaController::subtitleFont() const {
    return m_subtitleFont;
}

void MediaController::setSubtitleFont(const QFont& font) {
    m_subtitleFontChanged = true;
    m_subtitleFont = font;
}

// --------------------------------- Title ---------------------------------- //
void MediaController::setCurrentTitle(int title) {
    DEBUG_BLOCK;
    m_currentTitle = title;

    auto err{0};
    int64_t id{title};
    switch(source().discType()) {
        case Cd:
            if((err = mpv_set_property(m_player, "playlist-pos", MPV_FORMAT_INT64, &id)))
                error() << "Failed to set track:" << mpv_error_string(err);
            return;
        case Dvd:
        case Vcd:
        case BluRay:
            if((err = mpv_set_property(m_player, "disc-title", MPV_FORMAT_INT64, &id)))
                error() << "Failed to set title:" << mpv_error_string(err);
            return;
        case NoDisc:
            warning() << "Current media source is not a CD, DVD or VCD!";
            return;
    }

    warning() << "MediaSource does not support setting of tile in this version of Phonon MPV!"
              << "Type is" << source().discType();
}

int MediaController::availableTitles() const {
    return m_availableTitles;
}

int MediaController::currentTitle() const {
    return m_currentTitle;
}

void MediaController::setAutoplayTitles(bool autoplay) {
    m_autoPlayTitles = autoplay;
}

bool MediaController::autoplayTitles() const {
    return m_autoPlayTitles;
}

void MediaController::refreshTitles() {
    auto err{0};
    int64_t titles{0};
    if((err = mpv_get_property(m_player, "disc-titles/count", MPV_FORMAT_INT64, &titles)))
        error() << "Failed to set title:" << mpv_error_string(err);
    m_availableTitles = titles;
    emit availableTitlesChanged(m_availableTitles);
}

// -------------------------------- Chapter --------------------------------- //
void MediaController::setCurrentChapter(int chapter) {
    m_currentChapter = chapter;
    auto err{0};
    int64_t id{chapter};
    if((err = mpv_set_property(m_player, "chapter", MPV_FORMAT_INT64, &id)))
        error() << "Failed to set chapter:" << mpv_error_string(err);
}

int MediaController::availableChapters() const {
    return m_availableChapters;
}

int MediaController::currentChapter() const {
    return m_currentChapter;
}

// We need to rebuild available chapters when title is changed
void MediaController::refreshChapters() {
    auto err{0};
    int64_t chapters{0};
    if((err = mpv_get_property(m_player, "chapters", MPV_FORMAT_INT64, &chapters)))
        error() << "Failed to get chapters:" << mpv_error_string(err);
    m_availableChapters = chapters;

    emit availableChaptersChanged(m_availableChapters);
}

// --------------------------------- Angle ---------------------------------- //
void MediaController::setCurrentAngle(int angle) {
    m_currentAngle = angle;
    auto err{0};
    int64_t id{angle};
    if((err = mpv_set_property(m_player, "angle", MPV_FORMAT_INT64, &id)))
        error() << "Failed to set angle:" << mpv_error_string(err);
}

int MediaController::availableAngles() const {
    return m_availableAngles;
}

int MediaController::currentAngle() const {
    return m_currentAngle;
}

void MediaController::refreshAngles() {
    int64_t angle{0};
    if(mpv_set_property(m_player, "angle", MPV_FORMAT_INT64, &angle))
        m_availableAngles = 1;
    else
        m_availableAngles = 0;
    emit availableAnglesChanged(m_availableAngles);
}
