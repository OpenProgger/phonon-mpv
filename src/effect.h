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

#ifndef PHONON_MPV_EFFECT_H
#define PHONON_MPV_EFFECT_H

#include "sinknode.h"
#include "effectmanager.h"

#include <phonon/effectinterface.h>
#include <phonon/effectparameter.h>

namespace Phonon::MPV {

    class EffectManager;

    /** \brief Effect implementation for Phonon-VLC
    *
    * There are methods to get or set the effect parameters, implemented for
    * the EffectInterface. See the Phonon documentation for details.
    *
    * As a sink node, it provides methods to handle the connection to a media object.
    *
    * An effect manager is the parent of each effect.
    *
    * \see EffectManager
    * \see VolumeFaderEffect
    */
    class Effect: public QObject, public SinkNode, public EffectInterface {
        Q_OBJECT
        Q_INTERFACES(Phonon::EffectInterface)
    public:

        Effect(EffectManager* p_em, int i_effectId, QObject* p_parent);
        ~Effect();

        void setupEffectParams();
        QList<EffectParameter> parameters() const;
        QVariant parameterValue(const EffectParameter& param) const;
        void setParameterValue(const EffectParameter& param, const QVariant& newValue);

        /** \reimp */
        void handleConnectToMediaObject(MediaObject* p_media_object);
        /** \reimp */
        void handleDisconnectFromMediaObject(MediaObject* p_media_object);

    private:

        EffectManager* p_effectManager;
        int i_effect_filter;
        EffectInfo::Type effect_type;
        QList<Phonon::EffectParameter> parameterList;
    };

} // Namespace Phonon::MPV

#endif // PHONON_MPV_EFFECT_H
