#pragma once

#include <QColor>
#include <QObject>

namespace deepin::lyrics {

class LyricsTokens final : public QObject
{
    Q_OBJECT

    Q_PROPERTY(int space1 READ space1 CONSTANT FINAL)
    Q_PROPERTY(int space2 READ space2 CONSTANT FINAL)
    Q_PROPERTY(int space3 READ space3 CONSTANT FINAL)
    Q_PROPERTY(int dockVisualHeight READ dockVisualHeight CONSTANT FINAL)
    Q_PROPERTY(int dockLyricWidthMin READ dockLyricWidthMin CONSTANT FINAL)
    Q_PROPERTY(int dockLyricWidthDefault READ dockLyricWidthDefault CONSTANT FINAL)
    Q_PROPERTY(int dockLyricWidthMax READ dockLyricWidthMax CONSTANT FINAL)
    Q_PROPERTY(int dockCloseHitSize READ dockCloseHitSize CONSTANT FINAL)
    Q_PROPERTY(int dockLyricRadius READ dockLyricRadius CONSTANT FINAL)
    Q_PROPERTY(int dockCurrentFontSize READ dockCurrentFontSize CONSTANT FINAL)
    Q_PROPERTY(int dockSecondaryFontSize READ dockSecondaryFontSize CONSTANT FINAL)
    Q_PROPERTY(int dockLineHeight READ dockLineHeight CONSTANT FINAL)
    Q_PROPERTY(int bodyFontSize READ bodyFontSize CONSTANT FINAL)
    Q_PROPERTY(bool reduceMotion READ reduceMotion NOTIFY motionChanged FINAL)
    Q_PROPERTY(int motionFast READ motionFast NOTIFY motionChanged FINAL)
    Q_PROPERTY(int motionStandard READ motionStandard NOTIFY motionChanged FINAL)
    Q_PROPERTY(int motionSlow READ motionSlow NOTIFY motionChanged FINAL)
    Q_PROPERTY(QColor textPrimary READ textPrimary NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textSecondary READ textSecondary NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor textTertiary READ textTertiary NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor surfaceDock READ surfaceDock NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor surfaceDockHover READ surfaceDockHover NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor borderSubtle READ borderSubtle NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor accent READ accent NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor statusSuccess READ statusSuccess NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor statusWarning READ statusWarning NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor statusError READ statusError NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor dockLyricCurrentColor READ dockLyricCurrentColor NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor dockLyricSecondaryColor READ dockLyricSecondaryColor NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor dockLyricProgressColor READ dockLyricProgressColor NOTIFY paletteChanged FINAL)
    Q_PROPERTY(QColor dockLyricTrackColor READ dockLyricTrackColor NOTIFY paletteChanged FINAL)

public:
    explicit LyricsTokens(QObject *parent = nullptr);

    int space1() const;
    int space2() const;
    int space3() const;
    int dockVisualHeight() const;
    int dockLyricWidthMin() const;
    int dockLyricWidthDefault() const;
    int dockLyricWidthMax() const;
    int dockCloseHitSize() const;
    int dockLyricRadius() const;
    int dockCurrentFontSize() const;
    int dockSecondaryFontSize() const;
    int dockLineHeight() const;
    int bodyFontSize() const;
    bool reduceMotion() const;
    int motionFast() const;
    int motionStandard() const;
    int motionSlow() const;
    QColor textPrimary() const;
    QColor textSecondary() const;
    QColor textTertiary() const;
    QColor surfaceDock() const;
    QColor surfaceDockHover() const;
    QColor borderSubtle() const;
    QColor accent() const;
    QColor statusSuccess() const;
    QColor statusWarning() const;
    QColor statusError() const;
    QColor dockLyricCurrentColor() const;
    QColor dockLyricSecondaryColor() const;
    QColor dockLyricProgressColor() const;
    QColor dockLyricTrackColor() const;

public slots:
    void refresh();

signals:
    void paletteChanged();
    void motionChanged();

private:
    bool m_reduceMotion = false;
};

} // namespace deepin::lyrics
