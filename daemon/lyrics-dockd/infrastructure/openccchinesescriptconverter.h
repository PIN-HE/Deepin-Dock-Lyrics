#pragma once

#include "ports/chinesescriptconverter.h"

namespace deepin::lyrics {

class OpenCcChineseScriptConverter final : public ChineseScriptConverter
{
public:
    OpenCcChineseScriptConverter();
    ~OpenCcChineseScriptConverter() override;

    OpenCcChineseScriptConverter(const OpenCcChineseScriptConverter &) = delete;
    OpenCcChineseScriptConverter &operator=(const OpenCcChineseScriptConverter &) = delete;

    bool isValid() const;
    std::optional<QString> toSimplified(const QString &text) const override;

private:
    void *m_converter = nullptr;
};

} // namespace deepin::lyrics
