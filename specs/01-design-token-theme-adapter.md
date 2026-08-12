# S01 Design Token 与主题适配

**状态：** 待实施

**前置：** S00
**后续：** S06、S07

## 目标

将 [Design Token 规范](../DOCK_LYRICS_DESIGN_TOKENS_ZH.md) 落地为一个只读、主题响应式的 QML 单例 `LyricsTokens`，让 Dock Applet 与 DTK6 设置应用通过语义令牌取色和取尺寸。

## 范围

- 提供原始几何令牌：4 px 间距、36 px Dock 视觉高度、220-360 px 歌词宽度、28 px 图标点击区、字号和三档动效。
- 提供语义颜色令牌：主/次文本、弱文本、Dock 表面、边框、强调色、成功/警告/错误色以及歌词专用颜色。
- 用 C++ Theme Adapter 从 `DGuiApplicationHelper::applicationPalette()` / `DPalette` 获取颜色，并在主题或调色板变化时通知 QML。
- 提供 `reduceMotion`。系统减少动态效果启用时，所有 `motionFast`、`motionStandard`、`motionSlow` 返回 `0`。
- 把令牌注册为 `org.deepin.lyricsdock 1.0` 中可导入的 QML 单例，属性只读。

## 非范围

- 不复制一套固定浅色/深色 HEX 调色板。
- 不修改系统全局调色板、强调色或图标搜索路径。
- 不为设置应用绘制替代 DTK 原生控件的 Web 风格控件。

## 接口

```qml
import org.deepin.lyricsdock 1.0

Rectangle {
    height: LyricsTokens.dockVisualHeight
    color: LyricsTokens.dockLyricBackground
    radius: LyricsTokens.dockLyricRadius
}
```

最小属性集合：

| 分类 | 属性 |
|---|---|
| 尺寸 | `space1`、`space2`、`space3`、`dockVisualHeight`、`dockLyricWidthMin`、`dockLyricWidthMax`、`dockCloseHitSize` |
| 字体 | `dockCurrentFontSize`、`dockSecondaryFontSize`、`dockLineHeight`、`bodyFontSize` |
| 动效 | `motionFast`、`motionStandard`、`motionSlow`、`reduceMotion` |
| 颜色 | `textPrimary`、`textSecondary`、`textTertiary`、`surfaceDock`、`surfaceDockHover`、`borderSubtle`、`accent`、`statusWarning`、`statusError` |
| 歌词 | `dockLyricCurrentColor`、`dockLyricSecondaryColor`、`dockLyricProgressColor`、`dockLyricTrackColor` |

颜色映射应优先采用 `DPalette::TextTitle`、`TextTips`、`ItemBackground`、`FrameBorder`、`TextWarning` 等语义色；强调和状态色必须通过 DTK 主题 API 获取或基于主题色的受控派生，不能在业务 QML 写裸露 `#RRGGBB`。

## 行为与边界

- 主题切换或应用调色板变化后，单例发出相应的属性变更信号；已加载 QML 必须自动重绘。
- 固定格式组件不得因为文本、动画或主题变化改变其 `implicitWidth` / `implicitHeight`。
- 图标使用 DCI/主题图标基础名称；图标路径不进入 Token。
- 令牌只定义视觉语义。`LyricsTokens` 不读取 D-Bus，不处理歌词状态。

## 验收标准

1. Dock Applet 的业务 QML 中不存在用于 UI 的裸露 HEX 色值。
2. 在当前系统亮、暗主题下，文本、边框、表面和进度色均来自 DTK 主题，且应用无需重启即可更新。
3. `reduceMotion` 为真时，歌词切换和按钮悬停动画时长为 `0`。
4. Dock 关闭按钮的鼠标/触摸命中区域固定为 28 px，图标本身可为 16 px。
