/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>

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

#include "effect.h"

#include "effectmanager.h"

#include "mediaobject.h"

using namespace Phonon::MPV;

Effect::Effect(EffectManager* p_em, int i_effectId, QObject* p_parent): QObject(p_parent), SinkNode() {
    Q_UNUSED(p_em);
    Q_UNUSED(i_effectId);
}

Effect::~Effect() {
    parameterList.clear();
}

void Effect::handleConnectToMediaObject(MediaObject*) {
    switch(effect_type) {
        case EffectInfo::AudioEffect:
            break;
        case EffectInfo::VideoEffect:
            break;
    }
}

void Effect::handleDisconnectFromMediaObject(MediaObject*) {
    switch(effect_type) {
        case EffectInfo::AudioEffect:
            break;
        case EffectInfo::VideoEffect:
            break;
    }
}

void Effect::setupEffectParams() {
    switch(effect_type) {
        case EffectInfo::AudioEffect:
            break;
        case EffectInfo::VideoEffect:
            break;
    }
}

QList<Phonon::EffectParameter> Effect::parameters() const {
    return parameterList;
}

QVariant Effect::parameterValue(const Phonon::EffectParameter& param) const {
    Q_UNUSED(param);
    return QVariant();
}

void Effect::setParameterValue(const Phonon::EffectParameter& param, const QVariant& newValue) {
    Q_UNUSED(param);
    Q_UNUSED(newValue);
}
