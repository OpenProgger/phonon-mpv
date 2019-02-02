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

#include "volumefadereffect.h"

#define MPV_ENABLE_DEPRECATED 0
#include <mpv/client.h>

#include "utils/debug.h"

#include <QtCore/QTimeLine>

#ifndef QT_NO_PHONON_VOLUMEFADEREFFECT
using namespace Phonon::MPV;

VolumeFaderEffect::VolumeFaderEffect(QObject *parent)
    : QObject(parent)
    , SinkNode()
    , m_fadeCurve{Phonon::VolumeFaderEffect::Fade3Decibel}
    , m_fadeFromVolume(0)
    , m_fadeToVolume(0) {
    m_fadeTimeline = new QTimeLine(1000, this);
    connect(m_fadeTimeline, SIGNAL(valueChanged(qreal)), this, SLOT(slotSetVolume(qreal)));
}

VolumeFaderEffect::~VolumeFaderEffect() {
}

float VolumeFaderEffect::volume() const {
    Q_ASSERT(m_player);
    double vol{0.0};
    auto err{0};
    if((err = mpv_get_property(m_player, "volume", MPV_FORMAT_DOUBLE, &vol)))
        warning() << "Failed to get volume:" << mpv_error_string(err);
    return vol / 100.0f;
}

void VolumeFaderEffect::slotSetVolume(qreal volume) {
    setVolumeInternal(m_fadeFromVolume + (volume * (m_fadeToVolume - m_fadeFromVolume)));
}

Phonon::VolumeFaderEffect::FadeCurve VolumeFaderEffect::fadeCurve() const {
    return m_fadeCurve;
}

void VolumeFaderEffect::setFadeCurve(Phonon::VolumeFaderEffect::FadeCurve pFadeCurve) {
    m_fadeCurve = pFadeCurve;
    QEasingCurve fadeCurve;
    switch(pFadeCurve) {
        case Phonon::VolumeFaderEffect::Fade3Decibel:
            fadeCurve = QEasingCurve::InQuad;
            break;
        case Phonon::VolumeFaderEffect::Fade6Decibel:
            fadeCurve = QEasingCurve::Linear;
            break;
        case Phonon::VolumeFaderEffect::Fade9Decibel:
            fadeCurve = QEasingCurve::OutCubic;
            break;
        case Phonon::VolumeFaderEffect::Fade12Decibel:
            fadeCurve = QEasingCurve::OutQuart;
            break;
    }
    m_fadeTimeline->setEasingCurve(fadeCurve);
}

void VolumeFaderEffect::fadeTo(float targetVolume, int fadeTime) {
    Q_ASSERT(m_player);
    abortFade();
    m_fadeToVolume = targetVolume;
    m_fadeFromVolume = volume();

    // Don't call QTimeLine::setDuration() with zero.
    // It is not supported and breaks fading.
    if(fadeTime <= 0) {
        debug() << "Called with retarded fade time " << fadeTime;
        setVolumeInternal(targetVolume);
        return;
    }

    m_fadeTimeline->setDuration(fadeTime);
    m_fadeTimeline->start();
}

void VolumeFaderEffect::setVolume(float v) {
    abortFade();
    setVolumeInternal(v);
}

void VolumeFaderEffect::abortFade() {
    m_fadeTimeline->stop();
}

void VolumeFaderEffect::setVolumeInternal(float v) {
    if (m_player) {
        auto err{0};
        double volume{this->volume() * 100 * v};
        if(volume > 100.f)
            volume = 100.f;
        debug() << "Volume:" << volume;
        if((err = mpv_set_property(m_player, "volume", MPV_FORMAT_DOUBLE, &volume)))
            error() << "Failed to set volume:" << mpv_error_string(err);
    } else {
        warning() << Q_FUNC_INFO << this << "no m_player set";
    }
}

#endif //QT_NO_PHONON_VOLUMEFADEREFFECT
#include "moc_volumefadereffect.cpp"

