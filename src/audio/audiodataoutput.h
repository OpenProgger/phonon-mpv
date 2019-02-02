/*
    Copyright (C) 2006 Matthias Kretz <kretz@kde.org>
    Copyright (C) 2009 Martin Sandsmark <sandsmark@samfundet.no>
    Copyright (C) 2010 Ben Cooksley <sourtooth@gmail.com>
    Copyright (C) 2011 Harald Sitter <sitter@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) version 3, or any
    later version accepted by the membership of KDE e.V. (or its
    successor approved by the membership of KDE e.V.), Nokia Corporation
    (or its successors, if any) and the KDE Free Qt Foundation, which shall
    act as a proxy defined in Section 6 of version 3 of the license.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef Phonon_MPV_AUDIODATAOUTPUT_H
#define Phonon_MPV_AUDIODATAOUTPUT_H

#include <QtCore/QMutex>
#include <QtCore/QObject>

#include <phonon/audiodataoutput.h>
#include <phonon/audiodataoutputinterface.h>

#include "sinknode.h"

namespace Phonon::MPV {

    /** \brief Implementation for AudioDataOutput using libmpv(unfinished)
    *
    * This class makes the capture of raw audio data possible. It sets special options
    * for the libVLC Media Object when connecting to it, and then captures libVLC events
    * to get the audio data and send it further with the dataReady() signal.
    *
    * As a sink node, it can be connected to media objects.
    *
    * The frontend Phonon::AudioDataOutput object is unused.
    *
    * See the Phonon documentation for details.
    *
    * \see AudioOutput
    * \see SinkNode
    *
    * \author Martin Sandsmark <sandsmark@samfundet.no>
    */
    class AudioDataOutput : public QObject, public SinkNode, public AudioDataOutputInterface {
        Q_OBJECT
        Q_INTERFACES(Phonon::AudioDataOutputInterface)
    public:
        /**
        * Creates an audio data output. The sample rate is set to 44100 Hz.
        * The available audio channels are registered. These are:
        * \li Left \li Right \li Center \li LeftSurround \li RightSurround \li Subwoofer
        */
        explicit AudioDataOutput(QObject *parent);
        ~AudioDataOutput();

        Phonon::AudioDataOutput* frontendObject() const {
            return m_frontend;
        }

        void setFrontendObject(Phonon::AudioDataOutput* frontend) {
            m_frontend = frontend;
        }

    public Q_SLOTS:
        /**
        * \return The currently used number of samples passed through the signal.
        */
        int dataSize() const;

        /**
        * \return The current sample rate in Hz.
        */
        int sampleRate() const;

        /**
        * Sets the number of samples to be passed in one signal emission.
        */
        void setDataSize(int size);

    signals:
        void dataReady(const QMap<Phonon::AudioDataOutput::Channel, QVector<qint16> > &data);
        void dataReady(const QMap<Phonon::AudioDataOutput::Channel, QVector<float> > &data);
        void endOfMedia(int remainingSamples);
        void sampleReadDone();

    private Q_SLOTS:
        /**
        * Looks at the channel samples generated in lock() and creates the QMap required for
        * the dataReady() signal. Then the signal is emitted. This repeats as long as there is
        * data remaining.
        *
        * \see lock()
        */
        void sendData();

    private:
        /**
        * This is a VLC prerender callback. The m_locker mutex is locked, and a new buffer is prepared
        * for the incoming audio data.
        *
        * \param cw The AudioDataOutput for this callback
        * \param pcm_buffer The new data buffer
        * \param size Size for the incoming data
        *
        * \see unlock()
        */
        static void lock(AudioDataOutput* cw, quint8** pcm_buffer , quint32 size);

        /**
        * This is a VLC postrender callback. Interprets the data received in m_buffer,
        * separating the samples and channels. Finally, the buffer is freed and m_locker
        * is unlocked. Now the audio data output is ready for sending data.
        *
        * \param cw The AudioDataOutput for this callback
        *
        * \see lock()
        * \see sendData()
        */
        static void unlock(AudioDataOutput* cw, quint8* pcm_buffer,
                        quint32 channelCount, quint32 rate,
                        quint32 sampleCount, quint32 bits_per_sample,
                        quint32 size, qint64 pts);

        int m_dataSize;
        int m_sampleRate;
        Phonon::AudioDataOutput* m_frontend;

        QMutex m_locker;
        int m_channelCount;
        QVector<qint16> m_channelSamples[6];
        QList<Phonon::AudioDataOutput::Channel> m_channels;
    };

} // namespace Phonon::MPV

#endif // Phonon_MPV_AUDIODATAOUTPUT_H
