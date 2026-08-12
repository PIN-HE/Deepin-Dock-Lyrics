#pragma once

#include "lyricscore/lyricprovider.h"
#include "lyricscore/types.h"

namespace deepin::lyrics {

ParsedLyrics parseLyrics(const LyricPayload &payload);

} // namespace deepin::lyrics
