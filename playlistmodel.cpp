#include "playlistmodel.h"

#include <QRandomGenerator>

PlaylistModel::PlaylistModel(QObject* parent)
    : QObject(parent)
{
}

bool PlaylistModel::isEmpty() const
{
    return m_items.isEmpty();
}

int PlaylistModel::count() const
{
    return m_items.size();
}

int PlaylistModel::currentIndex() const
{
    return m_currentIndex;
}

bool PlaylistModel::hasCurrent() const
{
    return m_currentIndex >= 0 && m_currentIndex < m_items.size();
}

AudioTrack PlaylistModel::currentTrack() const
{
    return hasCurrent() ? m_items[m_currentIndex] : AudioTrack();
}

QUrl PlaylistModel::currentUrl() const
{
    return currentTrack().url;
}

AudioTrack PlaylistModel::at(int index) const
{
    return index >= 0 && index < m_items.size() ? m_items[index] : AudioTrack();
}

void PlaylistModel::add(const AudioTrack& track)
{
    m_items.append(track);
    emit changed();
}

bool PlaylistModel::updateTrack(int index, const AudioTrack& track)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }

    m_items[index] = track;
    emit changed();
    return true;
}

bool PlaylistModel::removeAt(int index)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }

    m_items.removeAt(index);

    int nextIndex = m_currentIndex;
    if (m_items.isEmpty()) {
        nextIndex = -1;
    } else if (index < m_currentIndex) {
        nextIndex = m_currentIndex - 1;
    } else if (index == m_currentIndex && m_currentIndex >= m_items.size()) {
        nextIndex = 0;
    }

    if (nextIndex != m_currentIndex) {
        m_currentIndex = nextIndex;
        emit currentIndexChanged(m_currentIndex);
    }

    emit changed();
    return true;
}

void PlaylistModel::clear()
{
    if (m_items.isEmpty() && m_currentIndex == -1) {
        return;
    }

    m_items.clear();
    if (m_currentIndex != -1) {
        m_currentIndex = -1;
        emit currentIndexChanged(m_currentIndex);
    }
    emit changed();
}

void PlaylistModel::setCurrentIndex(int index)
{
    const int nextIndex = (index >= 0 && index < m_items.size()) ? index : -1;
    if (m_currentIndex == nextIndex) {
        return;
    }

    m_currentIndex = nextIndex;
    emit currentIndexChanged(m_currentIndex);
}

bool PlaylistModel::updateTrackPlaybackStatus(int index,
                                              AudioTrackPlaybackStatus status,
                                              const QString& statusMessage)
{
    if (index < 0 || index >= m_items.size()) {
        return false;
    }

    AudioTrack& track = m_items[index];
    if (track.playbackStatus == status && track.statusMessage == statusMessage) {
        return true;
    }

    track.playbackStatus = status;
    track.statusMessage = statusMessage;
    emit changed();
    return true;
}

bool PlaylistModel::updateCurrentTrackPlaybackStatus(AudioTrackPlaybackStatus status,
                                                     const QString& statusMessage)
{
    return updateTrackPlaybackStatus(m_currentIndex, status, statusMessage);
}

bool PlaylistModel::moveToPrevious()
{
    if (m_items.isEmpty()) {
        return false;
    }

    if (m_currentIndex > 0) {
        setCurrentIndex(m_currentIndex - 1);
    } else {
        setCurrentIndex(m_items.size() - 1);
    }
    return true;
}

bool PlaylistModel::moveToNext()
{
    if (m_items.isEmpty()) {
        return false;
    }

    if (m_playMode == PlaylistRandom) {
        setCurrentIndex(QRandomGenerator::global()->bounded(m_items.size()));
    } else if (m_currentIndex < m_items.size() - 1) {
        setCurrentIndex(m_currentIndex + 1);
    } else {
        setCurrentIndex(0);
    }

    return true;
}

PlaylistPlayMode PlaylistModel::playMode() const
{
    return m_playMode;
}

void PlaylistModel::setPlayMode(PlaylistPlayMode mode)
{
    if (m_playMode == mode) {
        return;
    }

    m_playMode = mode;
    emit playModeChanged(m_playMode);
}
