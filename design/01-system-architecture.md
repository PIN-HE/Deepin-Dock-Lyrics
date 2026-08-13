# 01 系统架构 / System Architecture

本文描述 V1 已实现的模块边界与依赖关系。
This document describes module boundaries and dependencies as implemented in V1.

## 1. 构建目标依赖图 / Build Target Dependency Graph

```mermaid
flowchart BT
    VER["Lyrics::Version<br/>INTERFACE"]
    CORE["Lyrics::Core<br/>types, lrcparser<br/>lyricsync, normalization"]
    LOG["Lyrics::Logging<br/>logengine"]
    UI["Lyrics::Ui<br/>lyricstokens<br/>qmlregistration"]

    MPRIS["Lyrics::Mpris<br/>mprisplayerdiscovery"]
    SVC["Lyrics::Service<br/>controller + adapters + ports"]
    DAEMON["lyrics-dockd<br/>可执行 / executable"]

    SVM["Lyrics::SettingsViewModel"]
    SWIN["Lyrics::SettingsWindow"]
    SAPP["deepin-lyrics-settings<br/>可执行 / executable"]

    DVM["Lyrics::DockViewModel"]
    DAPP["ds-lyrics-dock<br/>SHARED"]

    CORE --> MPRIS
    CORE --> SVC
    LOG --> SVC
    MPRIS --> SVC
    VER --> SVC
    SVC --> DAEMON
    VER --> DAEMON

    CORE --> SAPP
    SVM --> SWIN
    UI --> SWIN
    SWIN --> SAPP
    SVM --> SAPP
    UI --> SAPP
    VER --> SAPP

    UI --> DAPP
    DVM --> DAPP
```

**中文**

`Lyrics::Core` 是唯一被三个可执行目标共享的库，因此其公共头文件构成事实上的跨进程契约。`Lyrics::Service` 只被 daemon 使用，Applet 与设置应用不链接它——这保证 Shell 进程不会意外引入网络或 SQLite 依赖。

**English**

`Lyrics::Core` is the only library shared by all three executables, so its public headers form the de facto cross-process contract. `Lyrics::Service` is used solely by the daemon; neither the applet nor the settings app links against it, which guarantees the Shell process cannot accidentally pull in network or SQLite dependencies.

## 2. 外部依赖 / External Dependencies

```mermaid
flowchart LR
    subgraph QT["Qt 6"]
        Q1["Core"]
        Q2["DBus"]
        Q3["Network"]
        Q4["Sql"]
        Q5["Widgets"]
        Q6["Qml / Quick"]
    end

    subgraph DTK["DTK6"]
        D1["Dtk6::Core"]
        D2["Dtk6::Gui"]
        D3["Dtk6::Widget"]
        D4["Dtk6DConfig"]
    end

    subgraph OTHER["其他 / Other"]
        O1["DDEShell"]
        O2["PkgConfig::OpenCC"]
    end

    DAEMON["lyrics-dockd"]
    SETTINGS["deepin-lyrics-settings"]
    APPLET["ds-lyrics-dock"]

    Q1 --> DAEMON
    Q2 --> DAEMON
    Q3 --> DAEMON
    Q4 --> DAEMON
    D1 --> DAEMON
    D4 --> DAEMON
    O2 --> DAEMON

    Q1 --> SETTINGS
    Q2 --> SETTINGS
    Q5 --> SETTINGS
    D1 --> SETTINGS
    D2 --> SETTINGS
    D3 --> SETTINGS

    Q1 --> APPLET
    Q6 --> APPLET
    O1 --> APPLET
```

**中文**

依赖分布体现进程职责：只有 daemon 依赖 `Network`、`Sql` 与 `OpenCC`；只有 Applet 依赖 `DDEShell` 与 `Quick`；只有设置应用依赖 `Widgets`。三者交集仅 `Qt6::Core` 与 `Qt6::DBus`。

**English**

The dependency spread mirrors process responsibility: only the daemon needs `Network`, `Sql`, and `OpenCC`; only the applet needs `DDEShell` and `Quick`; only the settings app needs `Widgets`. Their intersection is just `Qt6::Core` and `Qt6::DBus`.

## 3. Daemon 内部分层 / Daemon Internal Layering

```mermaid
flowchart TB
    subgraph APP["application 层 / layer"]
        CTRL["LyricsServiceController<br/>状态机与编排 / state machine and orchestration"]
    end

    subgraph PORTS["ports 层：抽象接口 / abstract interfaces"]
        PP["PlayerPort"]
        LP["LyricsPort"]
        SP["SettingsPort"]
        HP["HttpTransport"]
        CP["LyricsCache"]
        SC["ChineseScriptConverter"]
    end

    subgraph INFRA["infrastructure 层：具体实现 / concrete implementations"]
        MPA["MprisPlayerAdapter"]
        LLA["LrclibLyricsAdapter"]
        DCA["DConfigSettingsAdapter"]
        QNT["QtNetworkTransport"]
        SLC["SqliteLyricsCache"]
        OCC["OpenCcChineseScriptConverter"]
        LPR["LRCLIBProvider"]
        LDA["LyricsDbusAdapter"]
    end

    CTRL --> PP
    CTRL --> LP
    CTRL --> SP
    CTRL --> SC

    MPA -.->|实现 / implements| PP
    LLA -.->|实现 / implements| LP
    DCA -.->|实现 / implements| SP
    QNT -.->|实现 / implements| HP
    SLC -.->|实现 / implements| CP
    OCC -.->|实现 / implements| SC

    LLA --> CP
    LLA --> LPR
    LPR --> HP
    LDA --> CTRL
```

**中文**

控制器只依赖 ports 层的抽象，不直接引用任何 infrastructure 类型。这使测试可以注入假实现——现有 15 项测试正是依此运作，`tests/` 中的假传输不会消耗 LRCLIB 配额。

注意 `LRCLIBProvider` 不是一个 port 实现，而是被 `LrclibLyricsAdapter` 组合使用的具体协议客户端。它依赖 `HttpTransport` port，故其本身也可测。

**English**

The controller depends only on port abstractions and references no infrastructure type directly. This is what lets tests inject fakes — the existing 15 tests work exactly this way, and the fake transports under `tests/` consume no LRCLIB quota.

Note that `LRCLIBProvider` is not a port implementation but a concrete protocol client composed into `LrclibLyricsAdapter`. It depends on the `HttpTransport` port, so it is itself testable.

## 4. 依赖注入装配 / Dependency Injection Wiring

```mermaid
flowchart TB
    M["main()<br/>daemon/lyrics-dockd/main.cpp"]

    M --> S1["QtLogSink → LogEngine"]
    M --> S2["DConfigSettingsAdapter"]
    M --> S3["MprisPlayerAdapter<br/>(sessionBus)"]
    M --> S4["QtNetworkTransport"]
    S4 --> S5["LRCLIBProvider(transport)"]
    M --> S6["SqliteLyricsCache"]
    S5 --> S7["LrclibLyricsAdapter<br/>(provider, cache, logger)"]
    S6 --> S7
    S1 --> S7
    M --> S8["OpenCcChineseScriptConverter"]

    S8 --> GATE{"isValid()？"}
    GATE -->|否 / No| EXIT3["退出码 3<br/>exit 3<br/>opencc-unavailable"]
    GATE -->|是 / Yes| S9["LyricsServiceController<br/>(player, settings, logger,<br/>scriptConverter, lyrics)"]

    S3 --> S9
    S2 --> S9
    S7 --> S9
    S9 --> S10["LyricsDbusAdapter<br/>(controller, sessionBus)"]

    S10 --> REG{"registerService()？"}
    REG -->|否 / No| EXIT2["退出码 2<br/>exit 2"]
    REG -->|是 / Yes| RUN["controller.start()<br/>app.exec()"]
```

**中文**

两处启动门禁值得注意：

- **OpenCC 不可用即退出（码 3）。** 繁简转换被视为必需能力，不降级运行。
- **D-Bus 服务名注册失败即退出（码 2）。** 避免出现无法被调用的僵尸进程。

此外 `serviceOwnershipLost` 信号连接到 `QCoreApplication::quit`，即服务名被抢占时主动退出，不与新实例竞争。

**English**

Two startup gates are notable:

- **Exit code 3 when OpenCC is unavailable.** Script conversion is treated as a required capability with no degraded mode.
- **Exit code 2 when D-Bus name registration fails.** This avoids a zombie process nothing can call.

Additionally, the `serviceOwnershipLost` signal is connected to `QCoreApplication::quit`, so the daemon exits voluntarily if its name is taken rather than competing with a new instance.

## 5. Applet 与 Shell 集成 / Applet and Shell Integration

```mermaid
flowchart LR
    DOCK["org.deepin.ds.dock<br/>Dock 主体 / host"]
    PKG["org.deepin.ds.lyrics-dock<br/>dockOrder: 24"]
    QML["main.qml<br/>+ LyricBar.qml<br/>+ LyricsPopup.qml<br/>+ MarqueeText.qml"]
    VM["LyricsDockViewModel<br/>C++ 桥接 / bridge"]
    TOK["LyricsTokens<br/>主题 Token / theme tokens"]

    DOCK -->|父级 / parent| PKG
    PKG --> QML
    QML <--> VM
    QML --> TOK
    VM -.->|会话 D-Bus / session D-Bus| DAEMON["lyrics-dockd"]
```

**中文**

Applet 通过 `ds_install_package` 安装为独立包，父级为 `org.deepin.ds.dock`，不修改 Dock 源码或其 `main.qml`。C++ 桥接层 `LyricsDockViewModel` 承担 D-Bus 通信与重试，QML 只消费属性。

`LyricsDockViewModel` 内含单次重试定时器（`m_stateRetryTimer`），用于 daemon 尚未就绪时重新获取状态——Applet 随 Shell 启动，可能早于 daemon。

**English**

The applet installs as its own package via `ds_install_package` with `org.deepin.ds.dock` as parent, modifying neither the Dock source nor its `main.qml`. The C++ bridge `LyricsDockViewModel` owns D-Bus communication and retry; the QML merely consumes properties.

`LyricsDockViewModel` holds a single-shot retry timer (`m_stateRetryTimer`) to re-fetch state when the daemon is not yet up — the applet starts with the Shell and may well precede the daemon.

## 6. 已知架构偏差 / Known Architectural Divergences

| 偏差 / Divergence | 现状 / Current | 相关问题 / Related Issue |
|---|---|---|
| 歌词源为单实现 / Single source implementation | `LyricsPort` 仅有 `LrclibLyricsAdapter` 一个实现 / only one implementation | [多源方案 §4](../DOCK_LYRICS_MULTISOURCE_PLAN.md) |
| 繁简转换位置 / Script conversion placement | 仅作用于显示副本，查询与打分未归一 / display copy only, not queries or scoring | [issues/001](../issues/001-cjk-text-score-always-zero.md) |
| 封面未接入 / Cover art absent | `mpris:artUrl` 未读取，无相关字段 / not read, no field exists | [多源方案 §9](../DOCK_LYRICS_MULTISOURCE_PLAN.md) |
| 候选无正文 / Candidates lack body | `LyricCandidate` 丢弃 payload / discards payload | [issues/007](../issues/007-candidate-list-lacks-lyric-preview.md) |
