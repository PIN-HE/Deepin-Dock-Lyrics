#include "lyricscore/lyricsync.h"

#include <algorithm>

namespace deepin::lyrics {

LyricFrame frameAt(const TrackIdentity &track,
                   const ParsedLyrics &lyrics,
                   qint64 playerPositionMs,
                   int userOffsetMs)
{
    LyricFrame frame;
    frame.track = track;
    frame.timing = lyrics.timing;

    if (lyrics.timing == TimingCapability::Plain) {
        frame.currentText = lyrics.plainText;
        return frame;
    }
    if (lyrics.timing != TimingCapability::Line || lyrics.lines.isEmpty())
        return frame;

    const qint64 effectivePositionMs = std::max<qint64>(0, playerPositionMs + userOffsetMs);
    const auto next = std::upper_bound(
        lyrics.lines.cbegin(), lyrics.lines.cend(), effectivePositionMs,
        [](qint64 position, const LyricLine &line) { return position < line.startMs; });

    if (next == lyrics.lines.cbegin()) {
        frame.secondaryText = next->text;
        return frame;
    }

    const auto current = std::prev(next);
    frame.lineIndex = static_cast<int>(std::distance(lyrics.lines.cbegin(), current));
    frame.currentText = current->text;
    if (next == lyrics.lines.cend()) {
        frame.lineProgress = 1.0;
        return frame;
    }

    frame.secondaryText = next->text;
    const qint64 durationMs = next->startMs - current->startMs;
    if (durationMs > 0) {
        frame.lineProgress = std::clamp(
            static_cast<double>(effectivePositionMs - current->startMs) / durationMs,
            0.0, 1.0);
    }
    return frame;
}

} // namespace deepin::lyrics
