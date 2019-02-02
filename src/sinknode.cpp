/*
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

#include "sinknode.h"

#include "utils/debug.h"
#include "mediaobject.h"

using namespace Phonon::MPV;

SinkNode::SinkNode(): m_mediaObject{nullptr}, m_player{nullptr} {
}

SinkNode::~SinkNode() {
    if(m_mediaObject)
        disconnectFromMediaObject(m_mediaObject);
}

void SinkNode::connectToMediaObject(MediaObject* mediaObject) {
    if(m_mediaObject)
        error() << Q_FUNC_INFO << "m_mediaObject already connected";

    m_mediaObject = mediaObject;
    m_player = mediaObject->player();;
    m_mediaObject->addSink(this);

    // ---> Global handling goes here! Above the derivee handle! <--- //

    handleConnectToMediaObject(mediaObject);
}

void SinkNode::disconnectFromMediaObject(MediaObject *mediaObject) {
    handleDisconnectFromMediaObject(mediaObject);

    // ---> Global handling goes here! Below the derivee handle! <--- //

    if(m_mediaObject != mediaObject)
        error() << Q_FUNC_INFO << "SinkNode was not connected to mediaObject";

    if(m_mediaObject)
        m_mediaObject->removeSink(this);

    m_mediaObject = nullptr;
    m_player = nullptr;
}
