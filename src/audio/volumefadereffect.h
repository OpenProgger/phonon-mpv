/*  This file is part of the KDE project.
 *
 *    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies).
 *    Copyright (C) 2013 Martin Sandsmark <martin.sandsmark@kde.org>
 *
 *    This library is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU Lesser General Public License as published by
 *    the Free Software Foundation, either version 2.1 or 3 of the License.
 *
 *    This library is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Lesser General Public License for more details.
 *
 *    You should have received a copy of the GNU Lesser General Public License
 *    along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PHONON_MPV_VOLUMEFADEREFFECT_H
#define PHONON_MPV_VOLUMEFADEREFFECT_H

#include <phonon/volumefaderinterface.h>

#include <QtCore/QTime>
#include <QtCore/QPointer>

#include "sinknode.h"

class QTimeLine;

namespace Phonon::MPV {

    class VolumeFaderEffect : public QObject, public SinkNode, public VolumeFaderInterface {
        Q_OBJECT
        Q_INTERFACES(Phonon::VolumeFaderInterface)

    public:
        explicit VolumeFaderEffect(QObject* parent = 0);
        ~VolumeFaderEffect();

        // VolumeFaderInterface:
        float volume() const;
        Phonon::VolumeFaderEffect::FadeCurve fadeCurve() const;
        void setFadeCurve(Phonon::VolumeFaderEffect::FadeCurve fadeCurve);
        void fadeTo(float volume, int fadeTime);
        void setVolume(float v);
        QPointer<MediaObject> mediaObject() { return m_mediaObject; }

    private slots:
        void slotSetVolume(qreal v);

    private:
        void abortFade();
        inline void setVolumeInternal(float v);

        Phonon::VolumeFaderEffect::FadeCurve m_fadeCurve;
        float m_fadeFromVolume;
        float m_fadeToVolume;
        QTimeLine* m_fadeTimeline;
    };

} // namespace Phonon::MPV

#endif // PHONON_MPV_VOLUMEFADEREFFECT_H
