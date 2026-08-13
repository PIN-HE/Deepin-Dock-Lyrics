#include "lyricscore/lyricsync.h"

#include <algorithm>
#include <cmath>

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
    if (current != lyrics.lines.cbegin())
        frame.previousText = std::prev(current)->text;
    if (!lyrics.translationLines.isEmpty()) {
        const auto translation = std::lower_bound(
            lyrics.translationLines.cbegin(), lyrics.translationLines.cend(), current->startMs,
            [](const LyricLine &line, qint64 startMs) { return line.startMs < startMs; });
        if (translation != lyrics.translationLines.cend()
            && translation->startMs == current->startMs) {
            frame.translationText = translation->text;
        } else {
            // Accept small timestamp rounding, but never align by index: missing translations
            // would shift every following row onto the wrong lyric.
            // 允许小范围时间戳取整，但不按行号对齐，避免缺少一行翻译后整体错位。
            const auto previous = translation == lyrics.translationLines.cbegin()
                ? translation : std::prev(translation);
            const LyricLine *nearest = nullptr;
            if (previous != lyrics.translationLines.cend())
                nearest = &*previous;
            if (translation != lyrics.translationLines.cend()
                && (!nearest || std::abs(translation->startMs - current->startMs)
                                  < std::abs(nearest->startMs - current->startMs)))
                nearest = &*translation;
            if (nearest && std::abs(nearest->startMs - current->startMs) <= 750)
                frame.translationText = nearest->text;
        }
    }
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
