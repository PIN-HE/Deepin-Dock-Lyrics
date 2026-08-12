#include "infrastructure/openccchinesescriptconverter.h"

#include <opencc/opencc.h>

#include <QByteArray>

namespace deepin::lyrics {

namespace {

opencc_t invalidConverter()
{
    return reinterpret_cast<opencc_t>(-1);
}

} // namespace

OpenCcChineseScriptConverter::OpenCcChineseScriptConverter()
    : m_converter(opencc_open(OPENCC_DEFAULT_CONFIG_TRAD_TO_SIMP))
{
    if (m_converter == invalidConverter())
        m_converter = nullptr;
}

OpenCcChineseScriptConverter::~OpenCcChineseScriptConverter()
{
    if (m_converter)
        opencc_close(m_converter);
}

bool OpenCcChineseScriptConverter::isValid() const
{
    return m_converter != nullptr;
}

std::optional<QString> OpenCcChineseScriptConverter::toSimplified(const QString &text) const
{
    if (!m_converter)
        return std::nullopt;
    if (text.isEmpty())
        return QString();

    const QByteArray source = text.toUtf8();
    char *converted = opencc_convert_utf8(m_converter, source.constData(), source.size());
    if (!converted)
        return std::nullopt;
    const QString result = QString::fromUtf8(converted);
    opencc_convert_utf8_free(converted);
    return result;
}

} // namespace deepin::lyrics
