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
    m_supportedMimeTypes = QMap<QString, bool>{
        {"application/mpeg4-iod", true},
        {"application/mpeg4-muxcodetable", true},
        {"application/mxf", true},
        {"application/ogg", false},
        {"application/ram", false},
        {"application/sdp", false},
        {"application/vnd.apple.mpegurl", true},
        {"application/vnd.ms-asf", true},
        {"application/vnd.ms-wpl", true},
        {"application/vnd.rn-realmedia", true},
        {"application/vnd.rn-realmedia-vbr", true},
        {"application/x-cd-image", false},
        {"application/x-extension-m4a", false},
        {"application/x-extension-mp4", false},
        {"application/x-flac", false},
        {"application/x-flash-video", true},
        {"application/x-matroska", true},
        {"application/x-ogg", false},
        {"application/x-quicktime-media-link", true},
        {"application/x-quicktimeplayer", true},
        {"application/x-shockwave-flash", true},
        {"application/xspf+xml", true},
        {"audio/3gpp", false},
        {"audio/3gpp2", false},
        {"audio/AMR", false},
        {"audio/AMR-WB", false},
        {"audio/aac", false},
        {"audio/ac3", false},
        {"audio/basic", false},
        {"audio/dv", false},
        {"audio/eac3", false},
        {"audio/flac", false},
        {"audio/m4a", false},
        {"audio/midi", false},
        {"audio/mp1", false},
        {"audio/mp2", false},
        {"audio/mp3", false},
        {"audio/mp4", false},
        {"audio/mpeg", false},
        {"audio/mpegurl", false},
        {"audio/mpg", false},
        {"audio/ogg", false},
        {"audio/opus", false},
        {"audio/scpls", false},
        {"audio/vnd.dolby.heaac.1", false},
        {"audio/vnd.dolby.heaac.2", false},
        {"audio/vnd.dolby.mlp", false},
        {"audio/vnd.dts", false},
        {"audio/vnd.dts.hd", false},
        {"audio/vnd.rn-realaudio", false},
        {"audio/vorbis", false},
        {"audio/wav", false},
        {"audio/webm", false},
        {"audio/x-aac", false},
        {"audio/x-adpcm", false},
        {"audio/x-aiff", false},
        {"audio/x-ape", false},
        {"audio/x-flac", false},
        {"audio/x-gsm", false},
        {"audio/x-it", false},
        {"audio/x-m4a", false},
        {"audio/x-matroska", false},
        {"audio/x-mod", false},
        {"audio/x-mp1", false},
        {"audio/x-mp2", false},
        {"audio/x-mp3", false},
        {"audio/x-mpeg", false},
        {"audio/x-mpegurl", false},
        {"audio/x-mpg", false},
        {"audio/x-ms-asf", false},
        {"audio/x-ms-asx", false},
        {"audio/x-ms-wax", false},
        {"audio/x-ms-wma", false},
        {"audio/x-musepack", false},
        {"audio/x-pn-aiff", false},
        {"audio/x-pn-au", false},
        {"audio/x-pn-realaudio", false},
        {"audio/x-pn-realaudio-plugin", false},
        {"audio/x-pn-wav", false},
        {"audio/x-pn-windows-acm", false},
        {"audio/x-real-audio", false},
        {"audio/x-realaudio", false},
        {"audio/x-s3m", false},
        {"audio/x-scpls", false},
        {"audio/x-shorten", false},
        {"audio/x-speex", false},
        {"audio/x-tta", false},
        {"audio/x-vorbis", false},
        {"audio/x-vorbis+ogg", false},
        {"audio/x-wav", false},
        {"audio/x-wavpack", false},
        {"audio/x-xm", false},
        {"image/vnd.rn-realpix", true},
        {"misc/ultravox", true},
        {"text/google-video-pointer", true},
        {"text/x-google-video-pointer", true},
        {"video/3gp", true},
        {"video/3gpp", true},
        {"video/3gpp2", true},
        {"video/avi", true},
        {"video/divx", true},
        {"video/dv", true},
        {"video/fli", true},
        {"video/flv", true},
        {"video/mp2t", true},
        {"video/mp4", true},
        {"video/mp4v-es", true},
        {"video/mpeg", true},
        {"video/mpeg-system", true},
        {"video/msvideo", true},
        {"video/ogg", true},
        {"video/quicktime", true},
        {"video/vnd.divx", true},
        {"video/vnd.mpegurl", true},
        {"video/vnd.rn-realvideo", true},
        {"video/webm", true},
        {"video/x-anim", true},
        {"video/x-avi", true},
        {"video/x-flc", true},
        {"video/x-fli", true},
        {"video/x-flv", true},
        {"video/x-m4v", true},
        {"video/x-matroska", true},
        {"video/x-mpeg", true},
        {"video/x-mpeg-system", true},
        {"video/x-mpeg2", true},
        {"video/x-ms-asf", true},
        {"video/x-ms-asf-plugin", true},
        {"video/x-ms-asx", true},
        {"video/x-ms-wm", true},
        {"video/x-ms-wmv", true},
        {"video/x-ms-wmx", true},
        {"video/x-ms-wvx", true},
        {"video/x-msvideo", true},
        {"video/x-nsv", true},
        {"video/x-ogm", true},
        {"video/x-ogm+ogg", true},
        {"video/x-theora", true},
        {"video/x-theora+ogg", true},
        {"x-content/audio-cdda", false},
        {"x-content/audio-player", false},
        {"x-content/video-dvd", true},
        {"x-content/video-svcd", true},
        {"x-content/video-vcd", true}
    };
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
        case VolumeFaderEffectClass:
            return new VolumeFaderEffect(parent);
        default:
            break;
    }

    warning() << "Backend class" << c << "is not supported by Phonon MPV :(";
    return 0;
}

QStringList Backend::availableMimeTypes() const {
    return m_supportedMimeTypes.keys();
}

const QMap<QString, bool>& Backend::mimeTypes() const {
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

        VolumeFaderEffect* effect{qobject_cast<VolumeFaderEffect*>(source)};
        if(effect) {
            sinkNode->connectToMediaObject(effect->mediaObject());
            return true;
        }
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

        VolumeFaderEffect* const effect{qobject_cast<VolumeFaderEffect*>(source)};
        if(effect) {
            sinkNode->disconnectFromMediaObject(effect->mediaObject());
            return true;
        }
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
