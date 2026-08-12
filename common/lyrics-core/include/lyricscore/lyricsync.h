#pragma once

#include "lyricscore/types.h"

namespace deepin::lyrics {

LyricFrame frameAt(const TrackIdentity &track,
                   const ParsedLyrics &lyrics,
                   qint64 playerPositionMs,
                   int userOffsetMs);

} // namespace deepin::lyrics
