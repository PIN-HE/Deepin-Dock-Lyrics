# 【deepin插件开发活动】Deepin Dock Lyrics —— 让歌词住进 deepin 任务栏

> 本文为论坛「AI 开发实验室」参赛帖草稿(标题格式:`【deepin插件开发活动】+作品名称`)。
> 截图占位处需在发帖前替换为 3–5 张实际演示截图。

| 项目 | 内容 |
|---|---|
| **作品名称** | Deepin Dock Lyrics(`deepin-dock-lyrics`) |
| **参赛方向** | 方向四 · 扩展 DDE Shell 的桌面能力(组合方向三 · DTK 创新原生应用) |
| **源码仓库** | <https://github.com/PIN-HE/Deepin-Dock-Lyrics> |
| **作品类型** | dde-shell Dock Applet + 用户会话后台服务(`lyrics-dockd`)+ DTK6 设置应用 |
| **开源协议** | MIT(OSI 批准) |
| **版本** | v1.0.0.2 |

## 一、作品简介

在 deepin/UOS v25 上实现桌面级歌词显示的完整闭环:

```
任意 MPRIS 音乐播放器 → 自动发现并跟随 → 后台检索/缓存歌词 → Dock 右侧逐行同步显示
                                ↕
                    DTK6 设置应用统一管理(选播放器、偏移、布局、候选确认)
```

产品本身不播放音乐、不管理音乐库、不要求登录——它只做一件事:**让歌词安静地住进任务栏**。
听歌时切到浏览器、写代码、看文档,歌词始终在 Dock 右侧同步滚动,不遮挡窗口、不打断工作流;
播放器未运行时歌词区域自动隐藏,Dock 布局无侵入(不修改 Dock 源码)。

💡 **需求来源**:桌面歌词是用户从 Windows/移动端迁移到 Linux 后的高频需求之一:
主流播放器的桌面歌词插件在 Wayland 下普遍失效或依赖悬浮窗,而 deepin 的 Dock 体系
(dde-shell)天然适合承载常驻信息。本项目探索的就是「Shell 原生歌词部件」这一形态——
不弹窗、不置顶、不抢占焦点。

📷 演示截图占位 1:Dock 右侧歌词跟随播放
📷 演示截图占位 2:卡拉OK 布局
📷 演示截图占位 3:DTK6 设置应用(播放器选择/偏移/布局)
📷 演示截图占位 4:候选歌词确认
📷 演示截图占位 5:音频可视化

## 二、功能特性

- ✅ **MPRIS 播放器自动发现**:枚举会话中所有 MPRIS 播放器,跟踪播放/暂停/拖动/切歌,
  已知播放器显示友好中文名(如"网易云音乐""端闱乐部")。
- ✅ **逐行同步歌词**:当前行高亮 + 下一行提示,行内平滑进度、长句自动滚动;
  经典/卡拉OK 两种 Dock 布局一键切换。
- ✅ **在线检索与本地缓存**:LRCLIB 唯一在线源,请求序列化 + 限流 + SQLite 本地缓存
  (含负结果冷却),支持候选歌词确认;本地 LRC 文件与播放器缓存歌词同样可用。
- ✅ **多源歌词协调**:本地文件 → 播放器缓存 → LRCLIB 三级来源按优先级自动切换。
- ✅ **繁体自动转简体**:OpenCC `t2s.json` 解析前转换,缓存保留来源原文,不影响复现。
- ✅ **MPRIS 定向音频可视化**(可选,PipeWire):只读取选中播放器精确匹配的音频流,
  内存级处理,绝不录音/保存/上传;无法精确匹配时不读取系统音频。
- ✅ **DTK6 设置应用**:启用开关、播放器选择、同步偏移(±)、可视化、布局、候选确认、
  清除缓存、关于对话框,全中文界面 + DCI 图标。
- ✅ **系统托盘菜单**:启用/打开设置/退出,Qt 原生 QSystemTrayIcon 实现。
- ✅ **隐私优先**:日志 allowlist 过滤,曲名/歌词/查询串/D-Bus 原始载荷一律不进日志;
  全项目零遥测上传。
- ✅ **安装即用**:`cmake --install` 一键部署;daemon 由 systemd 用户服务管理并支持
  D-Bus 自动激活,重启会话后自动恢复。

## 三、创新说明

| 创新点 | 说明 | 对应评审维度 |
|---|---|---|
| **Shell 原生歌词部件** | 不是悬浮窗/置顶窗,而是挂载在 dde-shell Dock(`org.deepin.ds.dock`)内的 Applet(`dockOrder: 24`),随 Dock 主题、大小、位置自适应,Wayland 下天然稳定 | 创新性、UI/UX |
| **行内进度平滑外推** | 播放器 Position 上报粒度可能很粗(如 ter-music 约 1 秒步进),daemon 按 Rate 在两次上报间外推位置,只有上报值追平/超过外推值时才重建基准,行内进度丝滑且永不回跳 | 最佳技术实现 |
| **隐私优先的架构设计** | 三层防线:① 日志 allowlist 拒绝敏感字段;② 零遥测上传;③ 音频可视化只匹配选中播放器的精确 D-Bus PID 流,绝不做"按应用名/标题猜音频流",匹配失败即放弃 | 安全性、真实使用价值 |
| **六边形架构(ports/adapters)** | daemon 业务状态机只依赖端口接口,HTTP/缓存/播放器/设置全部可替换;测试用假传输运行,**不消耗 LRCLIB 真实配额**,外设变更零业务改动 | 代码质量与可维护性 |
| **多源歌词无缝切换** | 本地 LRC → 播放器缓存 → LRCLIB 三级降级,离线也能显示本地歌词 | 功能完整性 |
| **缓存保留原文 + 繁转简展示** | 转换只发生在展示链路,缓存/复现链路保留来源原文,功能与合规兼得 | 创新性 |

## 四、技术架构(简述)

- **三进程协作**:`lyrics-dockd`(会话 D-Bus 服务 `org.deepin.LyricsDock1`,承担全部网络/缓存/解析)
  + `deepin-lyrics-settings`(DTK6 控制界面)+ `org.deepin.ds.lyrics-dock`(dde-shell Applet 纯渲染)。
- **平台栈**:deepin 25 / Wayland · Qt 6(QML+Widgets 双栈)· DTK6(+DConfig)· dde-shell 2.x ·
  PipeWire(可选)· OpenCC · CMake/C++17。
- **工程质量**:约 20 个 CTest 测试目标,覆盖解析、同步、缓存、适配器、状态机与视图模型;
  测试零外部网络依赖。完整架构图与状态机见仓库 [design/](https://github.com/PIN-HE/Deepin-Dock-Lyrics/tree/master/design) 与 [README](https://github.com/PIN-HE/Deepin-Dock-Lyrics)。

## 五、deepin Skills 辅助开发说明

本项目全程基于 [deepin Skills](https://github.com/linuxdeepin/deepin-skills) 与 AI 编程 Agent 协作完成:

| Skill 模块 | 用途 |
|---|---|
| `dde-shell-development` | Dock Applet:DApplet 桥接、Shell QML API、`ds_install_package` 打包、`dockOrder` 布局 |
| `dtk-development` | DTK6 设置应用:DApplication/DAboutDialog、DTK 控件与主题、设计令牌 |
| `dde-tray-development` | 托盘交互约定参考 |
| `dde-control-center-development` | DConfig 配置规范参考 |
| `codebase-memory` | 代码库知识图谱:结构查询、调用链追踪、影响分析 |

> 按赛事要求,AI 编程工具中调用上述 Skills 的对话记录截图随本帖一并提交。

## 六、构建、安装与使用

```bash
cmake -S . -B build && cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure   # 单元测试
sudo cmake --install build                    # 安装设置应用 + Dock Applet + DConfig
```

daemon 与 systemd/D-Bus 服务文件按 README 三步安装后即用(`systemctl --user enable --now lyrics-dockd.service`)。
完整的分步安装、使用与卸载文档见仓库 [README](https://github.com/PIN-HE/Deepin-Dock-Lyrics)。

## 七、开源协议与致谢

- MIT 协议开源,欢迎 Star / Issue / PR。
- 感谢 deepin 社区与 deepin Skills 项目对独立开发者的支持。
