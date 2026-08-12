#include "lyricsui/lyricstokens.h"

#include <DGuiApplicationHelper>
#include <DPalette>

namespace deepin::lyrics {

namespace {

using Dtk::Gui::DGuiApplicationHelper;
using Dtk::Gui::DPalette;

QColor withOpacity(QColor color, qreal opacity)
{
    color.setAlphaF(opacity);
    return color;
}

DPalette currentPalette()
{
    return DGuiApplicationHelper::instance()->applicationPalette();
}

} // namespace

LyricsTokens::LyricsTokens(QObject *parent)
    : QObject(parent)
{
    auto *helper = DGuiApplicationHelper::instance();

    // 调色板变化必须使所有语义颜色的 QML 绑定重新求值。
    // Palette changes must re-evaluate every semantic color QML binding.
    connect(helper, &DGuiApplicationHelper::applicationPaletteChanged,
            this, &LyricsTokens::refresh);
    connect(helper, &DGuiApplicationHelper::themeTypeChanged,
            this, &LyricsTokens::refresh);

    refresh();
}

int LyricsTokens::space1() const
{
    return 4;
}

int LyricsTokens::space2() const
{
    return 8;
}

int LyricsTokens::space3() const
{
    return 12;
}

int LyricsTokens::space4() const
{
    return 16;
}

int LyricsTokens::settingsPagePadding() const
{
    return 24;
}

int LyricsTokens::settingsSectionGap() const
{
    return 24;
}

int LyricsTokens::settingsRowGap() const
{
    return space3();
}

int LyricsTokens::dockVisualHeight() const
{
    return 36;
}

int LyricsTokens::dockLyricWidthMin() const
{
    return 220;
}

int LyricsTokens::dockLyricWidthDefault() const
{
    return 280;
}

int LyricsTokens::dockLyricWidthMax() const
{
    return 360;
}

int LyricsTokens::dockCloseHitSize() const
{
    return 28;
}

int LyricsTokens::dockLyricRadius() const
{
    return 4;
}

int LyricsTokens::dockCurrentFontSize() const
{
    return 12;
}

int LyricsTokens::dockSecondaryFontSize() const
{
    return 11;
}

int LyricsTokens::dockLineHeight() const
{
    return 16;
}

int LyricsTokens::bodyFontSize() const
{
    return 14;
}

bool LyricsTokens::reduceMotion() const
{
    return m_reduceMotion;
}

int LyricsTokens::motionFast() const
{
    return m_reduceMotion ? 0 : 120;
}

int LyricsTokens::motionStandard() const
{
    return m_reduceMotion ? 0 : 180;
}

int LyricsTokens::motionSlow() const
{
    return m_reduceMotion ? 0 : 240;
}

QColor LyricsTokens::textPrimary() const
{
    return currentPalette().color(DPalette::TextTitle);
}

QColor LyricsTokens::textSecondary() const
{
    return currentPalette().color(DPalette::TextTips);
}

QColor LyricsTokens::textTertiary() const
{
    return currentPalette().color(DPalette::PlaceholderText);
}

QColor LyricsTokens::surfaceDock() const
{
    return currentPalette().color(DPalette::ItemBackground);
}

QColor LyricsTokens::surfaceDockHover() const
{
    return DGuiApplicationHelper::blendColor(surfaceDock(), withOpacity(accent(), 0.12));
}

QColor LyricsTokens::surfaceSelected() const
{
    return DGuiApplicationHelper::blendColor(
        currentPalette().color(QPalette::Base), withOpacity(accent(), 0.10));
}

QColor LyricsTokens::borderSubtle() const
{
    return currentPalette().color(DPalette::FrameBorder);
}

QColor LyricsTokens::accent() const
{
    return currentPalette().color(QPalette::Highlight);
}

QColor LyricsTokens::statusSuccess() const
{
    return currentPalette().color(DPalette::TextLively);
}

QColor LyricsTokens::statusWarning() const
{
    return currentPalette().color(DPalette::TextWarning);
}

QColor LyricsTokens::statusError() const
{
    return DGuiApplicationHelper::adjustColor(statusWarning(), -12, 16, -10, 18, -12, -12, 0);
}

QColor LyricsTokens::dockLyricCurrentColor() const
{
    return textPrimary();
}

QColor LyricsTokens::dockLyricSecondaryColor() const
{
    return textSecondary();
}

QColor LyricsTokens::dockLyricProgressColor() const
{
    return accent();
}

QColor LyricsTokens::dockLyricTrackColor() const
{
    return withOpacity(textPrimary(), 0.20);
}

void LyricsTokens::refresh()
{
    const bool nextReduceMotion = !DGuiApplicationHelper::testAttribute(
        DGuiApplicationHelper::HasAnimations);
    const bool motionChanged = m_reduceMotion != nextReduceMotion;
    m_reduceMotion = nextReduceMotion;

    emit paletteChanged();
    if (motionChanged)
        emit this->motionChanged();
}

} // namespace deepin::lyrics
