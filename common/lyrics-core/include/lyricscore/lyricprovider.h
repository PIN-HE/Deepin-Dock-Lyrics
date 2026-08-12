#pragma once

#include "lyricscore/types.h"

namespace deepin::lyrics {

struct LyricPayload {
    QString providerId;
    QString syncedLyrics;
    QString plainLyrics;
    TimingCapability timing = TimingCapability::None;
};

class LyricProvider
{
public:
    virtual ~LyricProvider() = default;

    virtual QString id() const = 0;
};

} // namespace deepin::lyrics
