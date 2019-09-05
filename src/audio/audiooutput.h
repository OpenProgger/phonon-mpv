/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2013 Harald Sitter <sitter@kde.org>

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

#ifndef PHONON_MPV_AUDIOOUTPUT_H
#define PHONON_MPV_AUDIOOUTPUT_H

#include <QObject>

#include <phonon/audiooutputinterface.h>

#include "sinknode.h"

namespace Phonon::MPV {

    /** \brief AudioOutput implementation for Phonon-VLC
    *
    * This class is a SinkNode that implements the AudioOutputInterface from Phonon. It
    * supports setting the volume and the audio output device.
    *
    * There are signals for the change of the volume or for when an audio device failed.
    *
    * See the Phonon::AudioOutputInterface documentation for details.
    *
    * \see AudioDataOutput
    */
    class AudioOutput : public QObject, public SinkNode, public AudioOutputInterface {
        Q_OBJECT
        Q_INTERFACES(Phonon::AudioOutputInterface)

    public:
        /**
        * Creates an AudioOutput with the given backend object. The volume is set to 1.0
        *
        * \param p_back Parent backend
        * \param p_parent A parent object
        */
        explicit AudioOutput(QObject* parent);
        ~AudioOutput();

        /** \reimp */
        void handleConnectToMediaObject(MediaObject* mediaObject) Q_DECL_OVERRIDE;

        /**
        * \return The current volume for this audio output.
        */
        qreal volume() const Q_DECL_OVERRIDE;

        /**
        * Sets the volume of the audio output. See the Phonon::AudioOutputInterface::setVolume() documentation
        * for details.
        */
        void setVolume(qreal volume) Q_DECL_OVERRIDE;

        /**
        * \return The index of the current audio output device from the list obtained from the backend object.
        */
        int outputDevice() const Q_DECL_OVERRIDE;

        /**
        * Sets the current output device for this audio output. The validity of the device index
        * is verified before attempting to change the device.
        *
        * \param device The index of the device, obtained from the backend's audio device list
        * \return \c true if succeeded, or no change was made
        * \return \c false if failed
        */
        bool setOutputDevice(int) Q_DECL_OVERRIDE;

        /**
        * Sets the current output device for this audio output.
        *
        * \param device The device to set; it should be valid and contain an usable deviceAccessList property
        * \return \c true if succeeded, or no change was made
        * \return \c false if failed
        */
        bool setOutputDevice(const AudioOutputDevice& newDevice) Q_DECL_OVERRIDE;

        void setStreamUuid(QString uuid) Q_DECL_OVERRIDE;

        void setMuted(bool mute) Q_DECL_OVERRIDE;

        virtual void setCategory(Phonon::Category category) Q_DECL_OVERRIDE;

    signals:
        void volumeChanged(qreal volume);
        void audioDeviceFailed();
        void mutedChanged(bool mute) Q_DECL_OVERRIDE;

    private slots:
        /**
        * Sets the volume to m_volume.
        */
        void onMutedChanged(bool mute);
        void onVolumeChanged(float volume);

    private:
        /**
        * We can only really set the output device once we have a libvlc_media_player, which comes
        * from our SinkNode.
        */
        void setOutputDeviceImplementation();

        qreal m_volume;
        bool m_muted;
        AudioOutputDevice m_device;
        QString m_streamUuid;
        Category m_category;
    };

} // namespace Phonon::MPV

#endif // PHONON_MPV_AUDIOOUTPUT_H
