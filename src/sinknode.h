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

#ifndef PHONON_MPV_SINKNODE_H
#define PHONON_MPV_SINKNODE_H

#include <QPointer>

struct mpv_handle;

namespace Phonon::MPV {

    class MediaObject;

    /** \brief The sink node is essentialy an output for a media object
    *
    * This class handles connections for the sink to a media object. It remembers
    * the media object and the libVLC media player associated with it.
    *
    * \see MediaObject
    */
    class SinkNode {
    public:
        SinkNode();
        virtual ~SinkNode();

        /**
        * Associates the sink node to the provided media object. The m_mediaObject and m_vlcPlayer
        * attributes are set, and the sink is added to the media object's sinks.
        *
        * \param mediaObject The media object to connect to.
        *
        * \see disconnectFromMediaObject()
        */
        void connectToMediaObject(MediaObject* mediaObject);

        /**
        * Removes this sink from the specified media object's sinks.
        *
        * \param mediaObject The media object to disconnect from
        *
        * \see connectToMediaObject()
        */
        void disconnectFromMediaObject(MediaObject* mediaObject);

    protected:
        /**
        * Handling function for derived classes.
        * \note This handle is executed *after* the global handle.
        *       Meaning the SinkNode base will be done handling the connect.
        * \see connectToMediaObject
        */
        virtual void handleConnectToMediaObject(MediaObject* mediaObject) { Q_UNUSED(mediaObject); }

        /**
        * Handling function for derived classes.
        * \note This handle is executed *before* the global handle.
        *       Meaning the SinkNode base will continue handling the disconnect.
        * \see disconnectFromMediaObject
        */
        virtual void handleDisconnectFromMediaObject(MediaObject* mediaObject) { Q_UNUSED(mediaObject); }

        /** Available while connected to a MediaObject (until disconnected) */
        QPointer<MediaObject> m_mediaObject;

        /** Available while connected to a MediaObject (until disconnected) */
        mpv_handle* m_player;
    };

} // namespace Phonon::MPV

#endif // PHONON_MPV_SINKNODE_H
