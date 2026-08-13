#pragma once

#include <lyricscore/types.h>

#include <QObject>

namespace deepin::lyrics {

// 外部歌词帧：由播放器内置歌词接口直接推送的行级快照。
// External lyric frame: a line-level snapshot pushed by a player's built-in
// lyric interface (e.g. Ter-Music's org.yxzl.ter_music.Lyrics).
struct ExternalLyricFrame {
    QString currentText;
    QString secondaryText;
    int lineIndex = -1;
    TimingCapability timing = TimingCapability::None;
    quint64 revision = 0;
    QString trackId;
};

// 外部帧源抽象端口。实现方负责：订阅/拉取外部歌词、判定当前选中播放器
// 是否属于本源、按 revision 去重，并以统一帧结构上报。
// External frame source port. Implementations subscribe to a player's own
// lyric interface, decide whether the selected player belongs to this source,
// deduplicate by revision, and report unified frames.
class ExternalFramePort : public QObject
{
    Q_OBJECT

public:
    explicit ExternalFramePort(QObject *parent = nullptr)
        : QObject(parent)
    {
    }

    // 由 Controller 在选中播放器变化时同步；实现方据此启停订阅。
    // Called by the controller whenever the selected player changes.
    virtual void setSelectedPlayer(const QString &busName) = 0;
    // 当前选中播放器是否属于本源的活跃状态（供 Controller 决定是否跳过查询链）。
    // Whether the source is active for the selected player (used by the
    // controller to skip the lyric lookup chain).
    virtual bool active() const = 0;

signals:
    void frameAvailable(const ExternalLyricFrame &frame);
    void stopped();
};

} // namespace deepin::lyrics

Q_DECLARE_METATYPE(deepin::lyrics::ExternalLyricFrame)
