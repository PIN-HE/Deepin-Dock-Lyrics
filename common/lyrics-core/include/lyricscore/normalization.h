#pragma once

#include "lyricscore/types.h"
#include "lyricscore/lyricprovider.h"

namespace deepin::lyrics {

QString normalizeText(const QString &text);
QString makeTrackKey(const TrackIdentity &track);
double scoreLyricCandidate(const TrackIdentity &track, const ProviderRecord &record);
QList<LyricCandidate> rankLyricCandidates(const TrackIdentity &track,
                                          const QList<ProviderRecord> &records,
                                          int limit = 20);

} // namespace deepin::lyrics
