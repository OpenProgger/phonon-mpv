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

#ifndef PHONON_MPV_MEDIACONTROLLER_H
#define PHONON_MPV_MEDIACONTROLLER_H

#include <phonon/AddonInterface>
#include <phonon/MediaSource>
#include <phonon/ObjectDescription>

#include <QtGui/QFont>

class QTimer;
struct mpv_handle;

namespace Phonon::MPV {

/**
 * \brief Interface for AddonInterface.
 *
 * Provides a bridge between Phonon's AddonInterface and MediaController.
 *
 * This class cannot inherit from QObject has MediaObject already inherit from QObject.
 * This is a Qt limitation: there is no possibility to inherit virtual Qobject :/
 * See http://doc.trolltech.com/qq/qq15-academic.html
 * Phonon implementation got the same problem.
 *
 * \see MediaObject
 */
class MediaController : public AddonInterface
{
public:

    MediaController();
    virtual ~MediaController();

    bool hasInterface(Interface iface) const;

    QVariant interfaceCall(Interface iface, int i_command, const QList<QVariant> & arguments = QList<QVariant>());

    /**
     * Overloaded by MediaObject through MediaObjectInterface.
     * Access to the media source is necessary to identify the type of the source
     * and behave accordingly.
     *
     * For example setTitle calls need to work on both DVDs and CDs, however
     * in libvlc titles and tracks are two different concepts.
     */
    virtual MediaSource source() const = 0;

    // MediaController signals
    virtual void availableSubtitlesChanged() = 0;
    virtual void availableAudioChannelsChanged() = 0;

    virtual void availableChaptersChanged(int) = 0;
    virtual void availableAnglesChanged(int) = 0;
    virtual void availableTitlesChanged(int) = 0;

    void titleAdded(int id, const QString &name);
    void chapterAdded(int titleId, const QString &name);

protected:
    // AudioChannel
    void setCurrentAudioChannel(const Phonon::AudioChannelDescription &audioChannel);
    QList<Phonon::AudioChannelDescription> availableAudioChannels() const;
    Phonon::AudioChannelDescription currentAudioChannel() const;
    void refreshAudioChannels();

    // Subtitle
    void setCurrentSubtitle(const Phonon::SubtitleDescription &subtitle);
    void setCurrentSubtitleFile(const QUrl &url);
    QList<Phonon::SubtitleDescription> availableSubtitles() const;
    Phonon::SubtitleDescription currentSubtitle() const;
    void refreshSubtitles();
    bool subtitleAutodetect() const;
    void setSubtitleAutodetect(bool enabled);
    QString subtitleEncoding() const;
    void setSubtitleEncoding(const QString &encoding);
    QFont subtitleFont() const;
    void setSubtitleFont(const QFont &font);

    // Chapter
    void setCurrentChapter(int chapterNumber);
    int availableChapters() const;
    int currentChapter() const;
    void refreshChapters();

    // Angles
    void setCurrentAngle(int chapterNumber);
    int availableAngles() const;
    int currentAngle() const;
    void refreshAngles();

    // Title
    void setCurrentTitle(int titleNumber);
    int availableTitles() const;
    int currentTitle() const;
    void setAutoplayTitles(bool autoplay);
    bool autoplayTitles() const;
    void refreshTitles();

    /**
     * Clear all member variables and emit appropriate signals.
     * This is used each time we restart the video.
     *
     * \see resetMembers
     */
    void resetMediaController();

    /**
     * Reset all member variables.
     *
     * \see resetMediaController
     */
    void resetMembers();

    Phonon::AudioChannelDescription m_currentAudioChannel;
    Phonon::SubtitleDescription m_currentSubtitle;

    int m_currentChapter;
    int m_availableChapters;

    int m_currentAngle;
    int m_availableAngles;

    int m_currentTitle;
    int m_availableTitles;

    bool m_autoPlayTitles;

    bool m_subtitleAutodetect;
    QString m_subtitleEncoding;
    bool m_subtitleFontChanged;
    QFont m_subtitleFont;

    // MediaPlayer
    mpv_handle* m_player;

    QTimer *m_refreshTimer;

    bool m_attemptingAutoplay;
};

} // namespace Phonon::MPV

#endif // PHONON_MPV_MEDIACONTROLLER_H
