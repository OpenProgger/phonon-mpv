/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>
    Copyright (C) 2011-2013 Harald Sitter <sitter@kde.org>

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

#include "backend.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QResizeEvent>
#include <QIcon>
#include <QLatin1Literal>
#include <QMessageBox>
#include <QtPlugin>
#include <QSettings>
#include <QVariant>

#include <phonon/GlobalDescriptionContainer>
#include <phonon/pulsesupport.h>

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/client.h>

#include <clocale>

#include "audio/audiooutput.h"
#include "audio/audiodataoutput.h"
#include "audio/volumefadereffect.h"
#include "effect.h"
#include "effectmanager.h"
#include "mediaobject.h"
#include "sinknode.h"
#include "utils/debug.h"
#include "video/videowidget.h"

using namespace Phonon::MPV;

Backend* Backend::self;

Backend::Backend(QObject* parent, const QVariantList&)
    : QObject(parent), m_mpvInstance{nullptr}, m_effectManager{nullptr} {
    self = this;

    // Backend information properties
    setProperty("identifier", QLatin1String("phonon_mpv"));
    setProperty("backendName", QLatin1String("MPV"));
    setProperty("backendComment", QLatin1String("mpv backend for Phonon"));
    setProperty("backendVersion", QLatin1String(PHONON_MPV_VERSION));
    setProperty("backendIcon", QLatin1String("mpv"));
    //setProperty("backendWebsite", QLatin1String("https://projects.kde.org/projects/kdesupport/phonon/phonon-mpv"));

    // Check if we should enable debug output
    auto debugLevel{qgetenv("PHONON_BACKEND_DEBUG").toInt()};
    if(debugLevel > 3) // 3 is maximum
        debugLevel = 3;
    Debug::setMinimumDebugLevel((Debug::DebugLevel)(Debug::DEBUG_NONE - 1 - debugLevel));

    debug() << "Constructing Phonon-MPV Version" << PHONON_MPV_VERSION;

    std::setlocale(LC_NUMERIC, "C");
    // Actual libMPV initialisation
    if(!(m_mpvInstance = mpv_create())) {
        fatal() << "libMPV: could not initialize";
        return;
    }

    // Ends up as something like $HOME/.config/Phonon/mpv.conf
    const auto configFileName{QSettings("Phonon", "mpv").fileName()};
    auto err{0};
    if(QFile::exists(configFileName)) {
        if((err = mpv_load_config_file(m_mpvInstance, configFileName.toLocal8Bit().data())))
            warning() << "Failed to apply config:" << mpv_error_string(err);
    }

    if(qgetenv("PHONON_SUBSYSTEM_DEBUG").toInt() > 0) {
        QByteArray input{"all=debug"};
        if((err = mpv_set_option_string(m_mpvInstance, "msg-level", input.constData())))
            warning() << "Failed to set Loglevel:" << mpv_error_string(err);
        QDir logFilePath{QDir::homePath().append("/.mpv")};
        logFilePath.mkdir("log");
        const auto logFile{logFilePath.path()
                .append("/log/mpv-log-")
                .append(QString::number(qApp->applicationPid()))
                .append(".txt")};
        if((err = mpv_set_option_string(m_mpvInstance, "log-file", logFile.toUtf8().constData())))
            warning() << "Failed to set Logfile:" << mpv_error_string(err);
    }

    // Create and initialize a libmpv instance (it should be done only once)
    if (mpv_initialize(m_mpvInstance) >= 0) {
        debug() << "Using MPV version" << mpv_client_api_version();
    } else {
        QMessageBox msg;
        msg.setIcon(QMessageBox::Critical);
        msg.setWindowTitle(tr("LibMPV Failed to Initialize"));
        msg.setText(tr("Phonon's VLC backend failed to start.\n\n"
                       "This usually means a problem with your mpv installation,"
                       " please report a bug at projects issue tracker."));
        msg.setDetailedText("Failed to create and initialize MPV Core Instance");
        msg.exec();
        fatal() << "Phonon::MPV::mpvInit: Failed to initialize mpv";
    }

    // until we have a video surface, disable video rendering
    if((err = mpv_set_property_string(m_mpvInstance, "vo", "null")))
        warning() << "failed to disable video rendering: " << mpv_error_string(err);
    PulseSupport* pulse{PulseSupport::getInstance()};
    pulse->enable(true);
    connect(pulse, SIGNAL(objectDescriptionChanged(ObjectDescriptionType)),
            SIGNAL(objectDescriptionChanged(ObjectDescriptionType)));

    QList<QString> deviceList;

    mpv_node audioDevices;
    if((err = mpv_get_property(m_mpvInstance, "audio-device-list", MPV_FORMAT_NODE, &audioDevices))) {
        warning() << "Failed to get audio devices:" << mpv_error_string(err);
        return;
    }

    QList<QByteArray> knownSoundSystems;
    // Whitelist - Order has no particular impact.
    // NOTE: if listing was not intercepted by the PA code above we also need
    //       to try injecting the pulse aout as otherwise the user would have to
    //       use the fake PA device in ALSA to output through PA (kind of silly).
    knownSoundSystems << QByteArray("pulse")
                      << QByteArray("alsa")
                      << QByteArray("oss")
                      << QByteArray("jack");
    QList<QByteArray> audioOutBackends;
    for(auto i{0}; i < audioDevices.u.list->num; i++) {
        foreach(const auto &soundSystem, knownSoundSystems) {
            if(QString(audioDevices.u.list->values[i].u.list->values[0].u.string).contains(soundSystem) && !audioOutBackends.contains(soundSystem))
                audioOutBackends.append(soundSystem);
        }
    }

#if (PHONON_VERSION < PHONON_VERSION_CHECK(4, 8, 51))
    if(pulse && pulse->isActive()) {
#else
    if(pulse && pulse->isUsable()) {
#endif
        if(audioOutBackends.contains("pulse")) {
            m_devices.append(QPair<QString, DeviceAccess>("Default", DeviceAccess("pulse", "default")));
#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 8, 51))
            pulse->request(true);
#endif
            return;
        } else {
            pulse->enable(false);
        }
    }

    foreach(const QByteArray &soundSystem, knownSoundSystems) {
        if(!audioOutBackends.contains(soundSystem)) {
            debug() << "Sound system" << soundSystem << "not supported by libmpv";
            continue;
        }
        const auto deviceCount{audioDevices.u.list->num};

        for(auto i{0}; i < deviceCount; i++) {
            QString idName(audioDevices.u.list->values[i].u.list->values[0].u.string);
            debug() << "found device" << soundSystem << idName;
            m_devices.append(QPair<QString, DeviceAccess>(idName == "auto" ? "default" : idName, DeviceAccess(soundSystem, idName)));
            debug() << "Added backend device" << idName;
        }

        // libmpv gives no devices for some sound systems, like OSS
        if(deviceCount == 0) {
            debug() << "manually injecting sound system" << soundSystem;
            // NOTE: Do not mark manually injected devices as advanced.
            //       libphonon filters advanced devices from the default
            //       selection which on systems such as OSX or Windows can
            //       lead to an empty device list as the injected device is
            //       the only available one.
            m_devices.append(QPair<QString, DeviceAccess>(QString::fromUtf8(soundSystem), DeviceAccess(soundSystem, "")));
        }
    }
    mpv_free_node_contents(&audioDevices);
    //m_effectManager = new EffectManager(this);
}

Backend::~Backend() {
    if(GlobalAudioChannels::self)
        delete GlobalAudioChannels::self;
    if(GlobalSubtitles::self)
        delete GlobalSubtitles::self;
    PulseSupport::shutdown();
}

QObject* Backend::createObject(BackendInterface::Class c, QObject* parent, const QList<QVariant>& args) {
    Q_UNUSED(args)
    if(!m_mpvInstance)
        return 0;

    switch(c) {
        case MediaObjectClass:
            return new MediaObject(parent);
        case AudioOutputClass:
            return new AudioOutput(parent);
            //FIXME
        /*case AudioDataOutputClass:
            return new AudioDataOutput(parent);
        case EffectClass:
            return effectManager()->createEffect(args[0].toInt(), parent);*/
        case VideoWidgetClass:
            return new VideoWidget(qobject_cast<QWidget*>(parent));
			//FIXME
        /*case VolumeFaderEffectClass:
            return new VolumeFaderEffect(parent);*/
        default:
            break;
    }

    warning() << "Backend class" << c << "is not supported by Phonon MPV :(";
    return 0;
}

QStringList Backend::availableMimeTypes() const {
    if(m_supportedMimeTypes.isEmpty())
        const_cast<Backend*>(this)->m_supportedMimeTypes = QStringList{
            "application/mpeg4-iod",
            "application/mpeg4-muxcodetable",
            "application/mxf",
            "application/ogg",
            "application/ram",
            "application/sdp",
            "application/vnd.apple.mpegurl",
            "application/vnd.ms-asf",
            "application/vnd.ms-wpl",
            "application/vnd.rn-realmedia",
            "application/vnd.rn-realmedia-vbr",
            "application/x-cd-image",
            "application/x-extension-m4a",
            "application/x-extension-mp4",
            "application/x-flac",
            "application/x-flash-video",
            "application/x-matroska",
            "application/x-ogg",
            "application/x-quicktime-media-link",
            "application/x-quicktimeplayer",
            "application/x-shockwave-flash",
            "application/xspf+xml",
            "audio/3gpp",
            "audio/3gpp2",
            "audio/AMR",
            "audio/AMR-WB",
            "audio/aac",
            "audio/ac3",
            "audio/basic",
            "audio/dv",
            "audio/eac3",
            "audio/flac",
            "audio/m4a",
            "audio/midi",
            "audio/mp1",
            "audio/mp2",
            "audio/mp3",
            "audio/mp4",
            "audio/mpeg",
            "audio/mpegurl",
            "audio/mpg",
            "audio/ogg",
            "audio/opus",
            "audio/scpls",
            "audio/vnd.dolby.heaac.1",
            "audio/vnd.dolby.heaac.2",
            "audio/vnd.dolby.mlp",
            "audio/vnd.dts",
            "audio/vnd.dts.hd",
            "audio/vnd.rn-realaudio",
            "audio/vorbis",
            "audio/wav",
            "audio/webm",
            "audio/x-aac",
            "audio/x-adpcm",
            "audio/x-aiff",
            "audio/x-ape",
            "audio/x-flac",
            "audio/x-gsm",
            "audio/x-it",
            "audio/x-m4a",
            "audio/x-matroska",
            "audio/x-mod",
            "audio/x-mp1",
            "audio/x-mp2",
            "audio/x-mp3",
            "audio/x-mpeg",
            "audio/x-mpegurl",
            "audio/x-mpg",
            "audio/x-ms-asf",
            "audio/x-ms-asx",
            "audio/x-ms-wax",
            "audio/x-ms-wma",
            "audio/x-musepack",
            "audio/x-pn-aiff",
            "audio/x-pn-au",
            "audio/x-pn-realaudio",
            "audio/x-pn-realaudio-plugin",
            "audio/x-pn-wav",
            "audio/x-pn-windows-acm",
            "audio/x-real-audio",
            "audio/x-realaudio",
            "audio/x-s3m",
            "audio/x-scpls",
            "audio/x-shorten",
            "audio/x-speex",
            "audio/x-tta",
            "audio/x-vorbis",
            "audio/x-vorbis+ogg",
            "audio/x-wav",
            "audio/x-wavpack",
            "audio/x-xm",
            "image/vnd.rn-realpix",
            "misc/ultravox",
            "text/google-video-pointer",
            "text/x-google-video-pointer",
            "video/3gp",
            "video/3gpp",
            "video/3gpp2",
            "video/avi",
            "video/divx",
            "video/dv",
            "video/fli",
            "video/flv",
            "video/mp2t",
            "video/mp4",
            "video/mp4v-es",
            "video/mpeg",
            "video/mpeg-system",
            "video/msvideo",
            "video/ogg",
            "video/quicktime",
            "video/vnd.divx",
            "video/vnd.mpegurl",
            "video/vnd.rn-realvideo",
            "video/webm",
            "video/x-anim",
            "video/x-avi",
            "video/x-flc",
            "video/x-fli",
            "video/x-flv",
            "video/x-m4v",
            "video/x-matroska",
            "video/x-mpeg",
            "video/x-mpeg-system",
            "video/x-mpeg2",
            "video/x-ms-asf",
            "video/x-ms-asf-plugin",
            "video/x-ms-asx",
            "video/x-ms-wm",
            "video/x-ms-wmv",
            "video/x-ms-wmx",
            "video/x-ms-wvx",
            "video/x-msvideo",
            "video/x-nsv",
            "video/x-ogm",
            "video/x-ogm+ogg",
            "video/x-theora",
            "video/x-theora+ogg",
            "x-content/audio-cdda",
            "x-content/audio-player",
            "x-content/video-dvd",
            "x-content/video-svcd",
            "x-content/video-vcd",
        };
    return m_supportedMimeTypes;
}

QList<int> Backend::objectDescriptionIndexes(ObjectDescriptionType type) const {
    QList<int> list;

    switch(type) {
        case Phonon::AudioChannelType:
            list << GlobalAudioChannels::instance()->globalIndexes();
            break;
        case Phonon::AudioOutputDeviceType:
        case Phonon::AudioCaptureDeviceType:
        case Phonon::VideoCaptureDeviceType: {
            QList<int> ids;
            for(auto i{0}; i < m_devices.size(); i++)
                    ids.append(i);
            return ids;
        }
        break;
        case Phonon::EffectType: /*{
            QList<EffectInfo> effectList{effectManager()->effects()};
            for (auto eff{0}; eff < effectList.size(); ++eff)
                list.append(eff);
            }*/
            break;
        case Phonon::SubtitleType:
            list << GlobalSubtitles::instance()->globalIndexes();
            break;
    }

    return list;
}

QHash<QByteArray, QVariant> Backend::objectDescriptionProperties(ObjectDescriptionType type, int index) const {
    QHash<QByteArray, QVariant> ret;

    switch(type) {
        case Phonon::AudioChannelType: {
            const AudioChannelDescription description = GlobalAudioChannels::instance()->fromIndex(index);
            ret.insert("name", description.name());
            ret.insert("description", description.description());
        }
        break;
        case Phonon::AudioOutputDeviceType:
        case Phonon::AudioCaptureDeviceType:
        case Phonon::VideoCaptureDeviceType: {
            // Index should be unique, even for different categories
            QHash<QByteArray, QVariant> properties;
            properties.insert("name", m_devices[index].first);
            properties.insert("description", "Detected MPV Device");
            properties.insert("isAdvanced", m_devices[index].first == "default" ? false : true);
            DeviceAccessList list;
            list.append(m_devices[index].second);
            properties.insert("deviceAccessList", QVariant::fromValue<Phonon::DeviceAccessList>(list));
            properties.insert("discovererIcon", "mpv");
            properties.insert("icon", QLatin1String("audio-card"));
            return properties;
        }
        break;
        case Phonon::EffectType: {
            /*const QList<EffectInfo> effectList{effectManager()->effects()};
            if(index >= 0 && index <= effectList.size()) {
                const EffectInfo& effect{effectList.at(index)};
                ret.insert("name", effect.name());
                ret.insert("description", effect.description());
                ret.insert("author", effect.author());
            } else {
                Q_ASSERT(1); // Since we use list position as ID, this should not happen
            }*/
        }
        break;
        case Phonon::SubtitleType: {
            const SubtitleDescription description{GlobalSubtitles::instance()->fromIndex(index)};
            ret.insert("name", description.name());
            ret.insert("description", description.description());
            ret.insert("type", description.property("type"));
        }
        break;
    }

    return ret;
}

bool Backend::startConnectionChange(QSet<QObject*> objects) {
    foreach(QObject* object, objects)
        debug() << "Object:" << object->metaObject()->className();

    return true;
}

bool Backend::connectNodes(QObject* source, QObject* sink) {
    debug() << "Backend connected" << source->metaObject()->className() << "to" << sink->metaObject()->className();

    SinkNode* sinkNode{dynamic_cast<SinkNode *>(sink)};
    if(sinkNode) {
        MediaObject* mediaObject{qobject_cast<MediaObject*>(source)};
        if(mediaObject) {
            // Connect the SinkNode to a MediaObject
            sinkNode->connectToMediaObject(mediaObject);
            return true;
        }

        /*VolumeFaderEffect* effect{qobject_cast<VolumeFaderEffect*>(source)};
        if(effect) {
            sinkNode->connectToMediaObject(effect->mediaObject());
            return true;
        }*/
    }

    warning() << "Linking" << source->metaObject()->className() << "to" << sink->metaObject()->className() << "failed";
    return false;
}

bool Backend::disconnectNodes(QObject* source, QObject* sink) {
    SinkNode* sinkNode{dynamic_cast<SinkNode*>(sink)};
    if(sinkNode) {
        MediaObject* const mediaObject{qobject_cast<MediaObject*>(source)};
        if(mediaObject) {
            // Disconnect the SinkNode from a MediaObject
            sinkNode->disconnectFromMediaObject(mediaObject);
            return true;
        }

        /*VolumeFaderEffect* const effect{qobject_cast<VolumeFaderEffect*>(source)};
        if(effect) {
            sinkNode->disconnectFromMediaObject(effect->mediaObject());
            return true;
        }*/
    }

    return false;
}

bool Backend::endConnectionChange(QSet<QObject*> objects) {
    foreach(QObject* object, objects)
        debug() << "Object:" << object->metaObject()->className();
    return true;
}

mpv_handle* Backend::handle() const {
    return m_mpvInstance;
}

EffectManager* Backend::effectManager() const {
    //return m_effectManager;
    return NULL;
}
