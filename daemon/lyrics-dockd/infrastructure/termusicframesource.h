#pragma once

#include "ports/externalframeport.h"

#include <QDBusConnection>
#include <QDBusServiceWatcher>
#include <QJsonObject>
#include <QObject>

namespace deepin::lyrics {

// Ter-Music 终端播放器的歌词帧源。
// Consumes Ter-Music's session D-Bus lyrics API:
//   bus org.mpris.MediaPlayer2.ter_music, interface org.yxzl.ter_music.Lyrics.
// Contract verified against src/org.yxzl.ter-music/media/session.c on the
// testing branch (2026-08-13).
class TerMusicFrameSource final : public ExternalFramePort
{
    Q_OBJECT

public:
    explicit TerMusicFrameSource(const QDBusConnection &connection, QObject *parent = nullptr);

    void setSelectedPlayer(const QString &busName) override;
    bool active() const override;

private slots:
    void onLyricsChanged(const QString &json);
    void onServiceOwnerChanged(const QString &, const QString &, const QString &newOwner);

private:
    void startListening();
    void stopListening();
    void requestSnapshot();
    void handleSnapshot(const QString &json);
    void parseSnapshot(const QJsonObject &object);
    void emitStopped();

    QDBusConnection m_connection;
    QDBusServiceWatcher m_watcher;
    QString m_selectedBusName;
    bool m_listening = false;
    bool m_active = false;
    quint64 m_lastRevision = 0;
    QString m_lastTrackId;
    bool m_hasTimestamps = false;
    ExternalLyricFrame m_pending;
};

} // namespace deepin::lyrics
