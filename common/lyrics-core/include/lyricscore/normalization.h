#pragma once

#include "lyricscore/types.h"

namespace deepin::lyrics {

QString normalizeText(const QString &text);
QString makeTrackKey(const TrackIdentity &track);

} // namespace deepin::lyrics
