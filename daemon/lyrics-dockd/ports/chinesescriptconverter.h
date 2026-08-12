#pragma once

#include <QString>

#include <optional>

namespace deepin::lyrics {

class ChineseScriptConverter
{
public:
    virtual ~ChineseScriptConverter() = default;

    virtual std::optional<QString> toSimplified(const QString &text) const = 0;
};

} // namespace deepin::lyrics
