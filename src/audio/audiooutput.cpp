/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2013-2018 Harald Sitter <sitter@kde.org>

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

#include "audiooutput.h"

#include <phonon/pulsesupport.h>

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/client.h>

#include "backend.h"
#include "utils/debug.h"
#include "mediaobject.h"

using namespace Phonon::MPV;

AudioOutput::AudioOutput(QObject* parent)
    : QObject(parent)
    , m_volume(1.0)
    , m_muted(false)
    , m_category(Phonon::NoCategory) {
}

AudioOutput::~AudioOutput() {
}

void AudioOutput::handleConnectToMediaObject(MediaObject* mediaObject) {
    Q_UNUSED(mediaObject);
    setOutputDeviceImplementation();
    if(!PulseSupport::getInstance()->isActive()) {
        // Rely on libmpv for updates if PASupport is not active
        connect(mediaObject, SIGNAL(mutedChanged(bool)),
                this, SLOT(onMutedChanged(bool)));
        connect(mediaObject, SIGNAL(volumeChanged(float)),
                this, SLOT(onVolumeChanged(float)));
    }
    PulseSupport* pulse{PulseSupport::getInstance()};
    if(pulse && pulse->isActive())
        pulse->setupStreamEnvironment(m_streamUuid);
}

qreal AudioOutput::volume() const {
    return m_volume;
}

void AudioOutput::setVolume(qreal volume) {
    if(m_player) {
        debug() << "async setting of volume to" << volume;
        const int preVolume = m_volume;
        m_volume = volume;
        double newVolume{m_volume * 100};
        if(newVolume > 100.f)
            newVolume = 100.f;
        auto err{0};
        if((err = mpv_set_property(m_player, "volume", MPV_FORMAT_DOUBLE, &newVolume)))
            error() << "Failed to set volume:" << mpv_error_string(err);

        debug() << "Volume changed from" << preVolume << "to" << newVolume;

        emit volumeChanged(m_volume);
    }
}

#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 8, 50))
void AudioOutput::setMuted(bool mute) {
    auto err{0};
    auto muted{0};
    if((err = mpv_get_property(m_player, "mute", MPV_FORMAT_FLAG, &muted)))
        warning() << "Failed to get volume:" << mpv_error_string(err);
    if(mute == static_cast<bool>(muted)) {
        // Make sure we actually have propagated the mutness into the frontend.
        onMutedChanged(mute);
        return;
    }
    muted = mute;
    if((err = mpv_set_property(m_player, "mute", MPV_FORMAT_FLAG, &muted)))
        warning() << "Failed to set volume:" << mpv_error_string(err);
}
#endif

void AudioOutput::setCategory(Category category) {
    m_category = category;
}

int AudioOutput::outputDevice() const {
    return m_device.index();
}

bool AudioOutput::setOutputDevice(int deviceIndex) {
    const auto device{AudioOutputDevice::fromIndex(deviceIndex)};
    if (!device.isValid()) {
        error() << Q_FUNC_INFO << "Unable to find the output device with index" << deviceIndex;
        return false;
    }
    return setOutputDevice(device);
}

#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 2, 0))
bool AudioOutput::setOutputDevice(const AudioOutputDevice &newDevice) {
    debug() << Q_FUNC_INFO;

    if(!newDevice.isValid()) {
        error() << "Invalid audio output device";
        return false;
    }
    if(newDevice == m_device)
        return true;
    m_device = newDevice;
    if(m_player)
        setOutputDeviceImplementation();

    return true;
}
#endif

#if (PHONON_VERSION >= PHONON_VERSION_CHECK(4, 6, 50))
void AudioOutput::setStreamUuid(QString uuid) {
    DEBUG_BLOCK;
    debug() << uuid;
    m_streamUuid = uuid;
}
#endif

void AudioOutput::setOutputDeviceImplementation() {
    Q_ASSERT(m_player);
    auto err{0};
    const auto pulseActive{PulseSupport::getInstance()->isActive()};
    if(pulseActive) {
        debug() << "Setting aout to pulse";
        if((err = mpv_set_property_string(m_player, "audio-device", "pulse")))
            warning() << "Failed to set pulse output:" << mpv_error_string(err);
        return;
    }

    const QVariant dalProperty{m_device.property("deviceAccessList")};
    if(!dalProperty.isValid()) {
        error() << "Device" << m_device.property("name") << "has no access list";
        return;
    }
    const auto deviceAccessList{dalProperty.value<DeviceAccessList>()};
    if(deviceAccessList.isEmpty()) {
        error() << "Device" << m_device.property("name") << "has an empty access list";
        return;
    }

    // ### we're not trying the whole access list (could mean same device on different soundsystems)
    const auto& firstDeviceAccess{deviceAccessList.first()};

    QByteArray soundSystem{firstDeviceAccess.first};
    QByteArray deviceName{firstDeviceAccess.second.toLatin1()};
    if(!deviceName.isEmpty()) {
        // print the name as possibly messed up by toLatin1() to see conversion problems
        debug() << "Setting output device to" << deviceName << '(' << m_device.property("name") << ')';
        if((err = mpv_set_property_string(m_player, "audio-device", soundSystem)))
            warning() << "Failed to set pulse output:" << mpv_error_string(err);
    }
}

void AudioOutput::onMutedChanged(bool mute) {
    m_muted = mute;
    emit mutedChanged(mute);
#if (PHONON_VERSION < PHONON_VERSION_CHECK(4, 8, 51))
    // Previously we had no interface signal to communicate mutness, so instead
    // emit volume.
    mute ? emit volumeChanged(0.0) : emit volumeChanged(volume());
#endif
}

void AudioOutput::onVolumeChanged(float volume) {
    m_volume = volume;
    emit volumeChanged(volume);
}
