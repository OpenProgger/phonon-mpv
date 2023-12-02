/*
    Copyright (C) 2007-2008 Tanguy Krotoff <tkrotoff@gmail.com>
    Copyright (C) 2008 Lukas Durfina <lukas.durfina@gmail.com>
    Copyright (C) 2009 Fathi Boudra <fabo@kde.org>
    Copyright (C) 2009-2011 vlc-phonon AUTHORS <kde-multimedia@kde.org>
    Copyright (C) 2010-2011 Harald Sitter <sitter@kde.org>

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

#ifndef PHONON_MPV_MEDIAOBJECT_H
#define PHONON_MPV_MEDIAOBJECT_H

#include <QObject>
#include <QTimer>

#include <phonon/mediaobjectinterface.h>
#include <phonon/addoninterface.h>

#include "mediacontroller.h"

namespace Phonon::MPV {

    class SinkNode;

    /** \brief Implementation for the most important class in Phonon
    *
    * The MediaObject class is the workhorse for Phonon. It handles what is needed
    * to play media. It has a media source object used to configure what media to
    * play.
    *
    * It provides the essential methods setSource(), play(), seek() and additional
    * methods to configure the next media source, the transition between sources,
    * transition times, ticks, other.
    *
    * There are numerous signals that provide information about the state of the media
    * and of the playing process. The aboutToFinish() and finished() signals are used
    * to see when the current media is finished.
    *
    * This class does not contain methods directly involved with libVLC. This part is
    * handled by the VLCMediaObject class. There are protected methods and slots
    * inherited by that class, like playInternal(), seekInternal().
    * These methods have no implementation here.
    *
    * For documentation regarding the methods implemented for MediaObjectInterface, see
    * the Phonon documentation.
    *
    * \see Phonon::MediaObjectInterface
    */
    class MediaObject : public QObject, public MediaObjectInterface, public MediaController {
        Q_OBJECT
        Q_INTERFACES(Phonon::MediaObjectInterface Phonon::AddonInterface)
        friend class SinkNode;

    public:
        /**
        * Initializes the members, connects the private slots to their corresponding signals,
        * sets the next media source to an empty media source.
        *
        * \param parent A parent for the QObject
        */
        explicit MediaObject(QObject* parent);
        ~MediaObject();

        inline mpv_handle* player() const { return m_player; }
        /**
        * Reset members (those that need resetting anyway).
        * Should always be called before going to a new source.
        */
        void resetMembers();

        /**
        * If the current state is paused, it resumes playing. Else, the playback
        * is commenced.
        */
        void play() Q_DECL_OVERRIDE;

        /// Pauses the playback for the media player.
        void pause() Q_DECL_OVERRIDE;

        /// Sets the next media source to an empty one and stops playback.
        void stop() Q_DECL_OVERRIDE;

        /// \returns \c true when there is a video available, \c false otherwise
        bool hasVideo() const Q_DECL_OVERRIDE;

        /// \returns \c true when the MediaObject is seekable, \c false otherwise
        bool isSeekable() const Q_DECL_OVERRIDE;

        /// \returns total time (length, duration) of the current MediaSource (-1 if unknown)
        qint64 totalTime() const Q_DECL_OVERRIDE;

        /// \returns An error message with the last libVLC error.
        QString errorString() const Q_DECL_OVERRIDE;

        /**
        * Adds a sink for this media object. During playInternal(), all the sinks
        * will have their addToMedia() called.
        *
        * \see playInternal()
        * \see SinkNode::addToMedia()
        */
        void addSink(SinkNode* node);

        /// Removes a sink from this media object.
        void removeSink(SinkNode* node);

        /**
        * Pushes a seek command to the SeekStack for this media object. The SeekStack then
        * calls seekInternal() when it's popped.
        */
        void seek(qint64 milliseconds) Q_DECL_OVERRIDE;

        /**
        * \return The interval between successive tick() signals. If set to 0, the emission
        * of these signals is disabled.
        */
        qint32 tickInterval() const Q_DECL_OVERRIDE;

        /**
        * Sets the interval between successive tick() signals. If set to 0, it disables the
        * emission of these signals.
        */
        void setTickInterval(qint32 tickInterval) Q_DECL_OVERRIDE;

        /**
        * \return The current time of the media, depending on the current state.
        * If the current state is stopped or loading, 0 is returned.
        * If the current state is error or unknown, -1 is returned.
        */
        qint64 currentTime() const Q_DECL_OVERRIDE;

        /// \return The current state for this media object.
        Phonon::State state() const Q_DECL_OVERRIDE;

        /// All errors are categorized as normal errors.
        Phonon::ErrorType errorType() const Q_DECL_OVERRIDE;

        /// \return The current media source for this media object.
        MediaSource source() const Q_DECL_OVERRIDE;

        /**
        * Sets the current media source for this media object. Depending on the source type,
        * the media object loads the specified media. The MRL is passed to loadMedia(), if the media
        * is not a stream. The currentSourceChanged() signal
        * is emitted.
        *
        * Supported media source types:
        * \li local files
        * \li URL
        * \li discs (CD, DVD, VCD)
        * \li capture devices (V4L)
        * \li streams
        *
        * \param source The media source that will become the current source.
        *
        * \see loadMedia()
        */
        void setSource(const MediaSource &source) Q_DECL_OVERRIDE;

        /// Sets the media source that will replace the current one, after the playback for it finishes.
        void setNextSource(const MediaSource &source) Q_DECL_OVERRIDE;

        qint32 prefinishMark() const Q_DECL_OVERRIDE;
        void setPrefinishMark(qint32 msecToEnd) Q_DECL_OVERRIDE;

        qint32 transitionTime() const Q_DECL_OVERRIDE;
        void setTransitionTime(qint32) Q_DECL_OVERRIDE;

        void emitAboutToFinish();
        void loadMedia(const QString& mrl);
        static void event_cb(void *opaque);

    Q_SIGNALS:
        // MediaController signals
        void availableSubtitlesChanged() Q_DECL_OVERRIDE;
        void availableAudioChannelsChanged() Q_DECL_OVERRIDE;

        void availableChaptersChanged(int) Q_DECL_OVERRIDE;
        void availableAnglesChanged(int) Q_DECL_OVERRIDE;
        void availableTitlesChanged(int) Q_DECL_OVERRIDE;

        void chapterChanged(int chapterNumber);
        void angleChanged(int angle);
        void titleChanged(int titleNumber);

        void mutedChanged(bool mute);
        void volumeChanged(float volume);

        void aboutToFinish();
        void bufferStatus(int percentFilled);
        void currentSourceChanged(const MediaSource& newSource);
        void finished();
        void hasVideoChanged(bool b_has_video);
        void metaDataChanged(const QMultiMap<QString, QString>& metaData);
        void prefinishMarkReached(qint32 msecToEnd);
        void seekableChanged(bool seekable);
        void stateChanged(Phonon::State newState, Phonon::State oldState);
        void tick(qint64 time);
        void totalTimeChanged(qint64 newTotalTime);

        void moveToNext();

    private Q_SLOTS:
        /**
        * If the new state is different from the current state, the current state is
        * changed and the corresponding signal is emitted.
        */
        void changeState(Phonon::State newState);

        /**
        * Checks when the tick(), prefinishMarkReached(), aboutToFinish() signals need to
        * be emitted and emits them if necessary.
        *
        * \param currentTime The current play time for the media, in miliseconds.
        */
        void timeChanged(qint64 time);
        void emitTick(qint64 time);

        /**
        * If the next media source is valid, the current source is replaced and playback is commenced.
        * The next source is set to an empty source.
        *
        * \see setNextSource()
        */
        void moveToNextSource();

        /** Retrieve meta data of a file (i.e ARTIST, TITLE, ALBUM, etc...). */
        void updateMetaData();
        void updateState(Phonon::State state);

        /** Called when the availability of video output changed */
        void onHasVideoChanged(bool hasVideo);

        /** Refreshes all MediaController descriptors if Video is present. */
        void refreshDescriptors();
        void mpv_event_loop();

    private:
        MediaSource m_nextSource;

        MediaSource m_mediaSource;
        Phonon::State m_state;

        qint32 m_prefinishMark;
        bool m_prefinishEmitted;

        bool m_aboutToFinishEmitted;

        qint32 m_tickInterval;
        qint64 m_lastTick;
        qint32 m_transitionTime;

        qint64 m_totalTime;
        QByteArray m_mrl;
        QList<SinkNode*> m_sinks;

        bool m_hasVideo;
        QMultiMap<QString, QString> m_mpvMetaData;

        /**
        * Workaround for being able to seek before VLC goes to playing state.
        * Seeks before playing are stored in this var, and processed on state change
        * to Playing.
        */
        qint64 m_seekpoint;

        bool m_buffering;
        Phonon::State m_stateAfterBuffering;
    };

} // namespace Phonon::MPV

#endif // PHONON_MPV_MEDIAOBJECT_H
