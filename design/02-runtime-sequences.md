# 02 运行时时序 / Runtime Sequences

本文的时序图对应 V1 已实现的调用链，常量取自源码。
The sequences here reflect call chains as implemented in V1, with constants taken from source.

## 1. Daemon 启动 / Daemon Startup

```mermaid
sequenceDiagram
    participant Main as main()
    participant OCC as OpenCcConverter
    participant Ctrl as ServiceController
    participant Dbus as DbusAdapter
    participant Bus as 会话总线 / Session Bus
    participant Mpris as MprisDiscovery
    participant Cfg as DConfig

    Main->>Main: 注册 DTK 控制台与文件日志<br/>register DTK appenders
    Main->>OCC: 构造 / construct
    OCC-->>Main: isValid()
    alt OpenCC 不可用 / unavailable
        Main->>Main: 退出码 3 / exit 3
    end
    Main->>Ctrl: 构造(player, settings, logger,<br/>scriptConverter, lyrics)
    Main->>Dbus: 构造(controller, sessionBus)
    Main->>Dbus: registerService()
    Dbus->>Bus: 请求名 org.deepin.LyricsDock1<br/>request name
    alt 注册失败 / failed
        Bus-->>Dbus: 拒绝 / denied
        Main->>Main: 退出码 2 / exit 2
    end
    Bus-->>Dbus: 已获得所有权 / owned
    Main->>Ctrl: start()
    Ctrl->>Cfg: 读取 enabled / player / offset<br/>read settings
    Cfg-->>Ctrl: 配置值 / values
    Ctrl->>Mpris: start()
    Mpris->>Bus: ListNames()
    Bus-->>Mpris: 总线名列表 / bus names
    Mpris->>Mpris: 筛选 org.mpris.MediaPlayer2.*<br/>filter prefix
    Mpris-->>Ctrl: availablePlayersChanged
    Ctrl->>Ctrl: refreshStatus()
    Ctrl-->>Dbus: stateChanged
```

**中文**

启动顺序的关键约束是**先注册 D-Bus 名再启动 MPRIS 发现**。若顺序颠倒，发现过程产生的状态变更会在服务名尚未就绪时发出信号，Applet 无法接收。

**English**

The critical ordering constraint is **register the D-Bus name before starting MPRIS discovery**. Reversed, state changes produced by discovery would emit signals before the service name exists, and the applet would miss them.

## 2. 切歌与歌词检索 / Track Change and Lyric Lookup

```mermaid
sequenceDiagram
    participant Player as MPRIS 播放器 / Player
    participant Mpris as MprisDiscovery
    participant Ctrl as ServiceController
    participant Adapter as LrclibAdapter
    participant Cache as SqliteCache
    participant Provider as LRCLIBProvider
    participant API as lrclib.net
    participant Dbus as DbusAdapter
    participant Applet as Dock Applet

    Player->>Mpris: PropertiesChanged(Metadata)
    Mpris->>Mpris: trackFromMetadata()<br/>提取 title/artist/album/length
    Mpris-->>Ctrl: snapshotChanged
    Ctrl->>Ctrl: setStatus(WaitingForTrack)<br/>m_trackStable = false
    Ctrl-->>Dbus: stateChanged
    Note over Mpris: 防抖 400 ms<br/>debounce 400 ms<br/>mprisplayerdiscovery.cpp:119

    Mpris->>Mpris: publishStableTrack()
    Mpris-->>Ctrl: trackChanged(track)
    Ctrl->>Ctrl: setStatus(LookingUpLyrics)
    Ctrl->>Adapter: search(track)

    Adapter->>Cache: open() → find(trackKey)
    alt 正缓存命中 / positive cache hit
        Cache-->>Adapter: 已缓存载荷 / cached payload
        Adapter-->>Ctrl: lyricsReady(payload)
    else 负缓存命中 / negative cache hit
        Cache-->>Adapter: isNegative == true
        Adapter-->>Ctrl: noLyrics()
    else 冷却中 / in cooldown
        Cache-->>Adapter: cooldownUntil > now
        Adapter-->>Ctrl: failed("rate-limited")
    else 缓存未命中 / cache miss
        Adapter->>Provider: getExact(track)
        Note over Provider: 串行队列，最小间隔 300 ms<br/>serial queue, min gap 300 ms<br/>lrclibprovider.cpp:15
        Provider->>API: GET /api/get<br/>track_name, artist_name,<br/>album_name, duration
        API-->>Provider: 200 + 记录 / record
        Provider-->>Adapter: ProviderResult(Success)
        Adapter->>Adapter: scoreLyricCandidate()<br/>durationEligible?
        alt score ≥ 0.85 且时长合格 / and duration ok
            Adapter->>Cache: store(trackKey, record)
            Adapter-->>Ctrl: lyricsReady(payload)
        else 未达阈值 / below threshold
            Adapter-->>Ctrl: failed("provider-failed")
            Note over Adapter,Ctrl: 缺陷：应降级到候选搜索<br/>DEFECT: should degrade to<br/>candidate search — issues/002
        end
    end

    Ctrl->>Ctrl: OpenCC 繁转简（仅显示副本）<br/>t2s on display copy only
    Ctrl->>Ctrl: parseLyrics() → m_parsedLyrics
    Ctrl->>Ctrl: setStatus(LyricsReady)
    Ctrl-->>Dbus: stateChanged + frameChanged
    Dbus-->>Applet: StateChanged / FrameChanged
```

**中文**

图中标注了 [issues/002](../issues/002-exact-match-failure-reported-as-error.md) 的缺陷位置：`/api/get` 成功返回歌词但评分未达阈值时，当前实现发出 `failed` 而非降级到候选搜索。降级路径仅在 HTTP 404 时触发。

**English**

The diagram marks where [issues/002](../issues/002-exact-match-failure-reported-as-error.md) sits: when `/api/get` returns lyrics successfully but the score falls short, the current implementation emits `failed` instead of degrading to candidate search. The degradation path fires only on HTTP 404.

## 3. 候选搜索与用户确认 / Candidate Search and User Confirmation

```mermaid
sequenceDiagram
    participant User as 用户 / User
    participant Settings as 设置应用 / Settings
    participant Dbus as DbusAdapter
    participant Ctrl as ServiceController
    participant Adapter as LrclibAdapter
    participant Provider as LRCLIBProvider
    participant API as lrclib.net
    participant Cache as SqliteCache

    alt 自动触发：/api/get 返回 404 / automatic on 404
        Adapter->>Adapter: beginCandidateSearch()
    else 手动触发 / manual
        User->>Settings: 点击"搜索候选"<br/>click Search Candidates
        Settings->>Dbus: SearchCandidates()
        Dbus->>Ctrl: searchCandidates()
        Ctrl->>Adapter: searchCandidates(track)
    end

    Adapter->>Provider: search(track)
    Provider->>API: GET /api/search<br/>track_name, artist_name, album_name
    API-->>Provider: 200 + 数组（上限 20 条）<br/>array, capped at 20
    Provider->>Provider: parseRecords()
    Provider-->>Adapter: ProviderResult(records)

    Adapter->>Adapter: rankLyricCandidates(track, records, 20)
    Note over Adapter: 过滤 score < 0.60<br/>filter score < 0.60<br/>normalization.cpp:92

    alt 无候选通过过滤 / none pass filter
        Adapter->>Cache: storeNegative(trackKey, "not-found")
        Adapter-->>Ctrl: noLyrics()
        Note over Adapter: 缺陷：艺名差异使正确匹配被丢弃<br/>DEFECT: artist variance drops<br/>correct matches — issues/003
    else 最高分 ≥ 0.85 且时长合格 / top ≥ 0.85 and duration ok
        Adapter->>Provider: getById(bestId)
        Provider->>API: GET /api/get/{id}
        API-->>Provider: 200 + 完整记录 / full record
        Adapter->>Cache: store(trackKey, record, score, false)
        Adapter-->>Ctrl: lyricsReady(payload)
    else 需用户确认 / needs confirmation
        Adapter->>Adapter: m_candidateIds.insert(每条 / each)
        Adapter-->>Ctrl: candidatesChanged(candidates)
        Ctrl->>Ctrl: setStatus(NeedsCandidateSelection)
        Ctrl-->>Dbus: CandidatesChanged
        Dbus-->>Settings: 候选列表 / candidate list
        Note over Settings: 仅元数据，无歌词正文<br/>metadata only, no lyric body<br/>issues/007
        Settings-->>User: 展示候选 / show candidates
        User->>Settings: 选择一条 / pick one
        Settings->>Dbus: SelectCandidate(providerId, candidateId)
        Dbus->>Ctrl: selectCandidate()
        Ctrl->>Adapter: selectCandidate()
        Adapter->>Adapter: 校验 m_candidateIds 包含<br/>verify membership
        Adapter->>Provider: getById(candidateId)
        Provider->>API: GET /api/get/{id}
        API-->>Provider: 200 + 记录 / record
        Adapter->>Cache: store(..., confidence=1.0,<br/>userConfirmed=true)
        Adapter-->>Ctrl: lyricsReady(payload)
    end
```

**中文**

`selectCandidate` 以 `confidence = 1.0` 与 `userConfirmed = true` 写入缓存，绕过评分门槛。这是用户手动搜索能成功而自动路径失败的直接原因。

`m_candidateIds` 成员用于校验用户提交的 `candidateId` 确实来自最近一次搜索结果，防止任意 ID 注入。

**English**

`selectCandidate` writes to the cache with `confidence = 1.0` and `userConfirmed = true`, bypassing the score gate. This is precisely why manual search succeeds where the automatic path fails.

The `m_candidateIds` member verifies that a user-submitted `candidateId` genuinely came from the most recent search, preventing arbitrary ID injection.

## 4. 播放中的帧发布 / Frame Publication During Playback

```mermaid
sequenceDiagram
    participant Player as MPRIS 播放器 / Player
    participant Mpris as MprisDiscovery
    participant Ctrl as ServiceController
    participant Sync as frameAt()
    participant Dbus as DbusAdapter
    participant Applet as Dock Applet

    loop 每 200 ms / every 200 ms
        Mpris->>Player: Get(Position)
        Note over Mpris: mprisplayerdiscovery.cpp:117
        Player-->>Mpris: positionUs
        Mpris->>Mpris: positionMs = positionUs / 1000
        Mpris-->>Ctrl: snapshotChanged
        Ctrl->>Ctrl: publishFrame()
        Ctrl->>Sync: frameAt(track, parsedLyrics,<br/>positionMs, offsetMs)
        Sync-->>Ctrl: LyricFrame<br/>currentText, secondaryText,<br/>lineIndex, lineProgress
        Ctrl-->>Dbus: frameChanged(frameMap)
        Dbus->>Dbus: queueFrame()
        Note over Dbus: 节流 200 ms 单次触发<br/>throttled 200 ms single-shot<br/>lyricsdbusadapter.cpp:41
        Dbus-->>Applet: FrameChanged(frame)
        Applet->>Applet: 更新 QML 属性 / update QML
    end
```

**中文**

位置采样与帧节流均为 200 ms，但两者不同步：`m_frameTimer` 是单次定时器，仅在有待发布帧时启动，因此暂停时不产生总线流量。

`frameAt()` 是纯函数，不含状态。行内进度 `lineProgress` 由当前行起始时间到下一行起始时间的区间线性插值得出——这是视觉进度，非真实逐字时间戳。

**English**

Position sampling and frame throttling are both 200 ms but are not synchronised: `m_frameTimer` is single-shot and starts only when a frame is pending, so a paused player generates no bus traffic.

`frameAt()` is a pure function with no state. The intra-line `lineProgress` is linearly interpolated between the current line's start and the next line's start — a visual progress indicator, not a real word-level timestamp.

## 5. 错误与限流路径 / Error and Rate-Limit Paths

```mermaid
sequenceDiagram
    participant Adapter as LrclibAdapter
    participant Provider as LRCLIBProvider
    participant API as lrclib.net
    participant Cache as SqliteCache
    participant Ctrl as ServiceController

    Adapter->>Provider: getExact / search / getById
    Provider->>API: HTTP GET（超时 10 s / timeout 10 s）

    alt HTTP 429
        API-->>Provider: 429 + Retry-After
        Provider->>Provider: m_cooldownUntil = retryAfter()<br/>钳制 1–3600 s / clamp
        Provider-->>Adapter: RateLimited + retryAt
        Adapter->>Cache: setCooldownUntil(retryAt)
        Adapter-->>Ctrl: failed("rate-limited")
    else 5xx 或网络错误，尝试数 < 2 / attempts < 2
        API-->>Provider: 500 / 网络错误 / network error
        Provider->>Provider: 指数退避 300 ms × 2^(n-1)<br/>exponential backoff
        Provider->>API: 重试 / retry
    else 5xx 或网络错误，已达上限 / attempts exhausted
        Provider-->>Adapter: NetworkError
        Adapter-->>Ctrl: failed("network-unavailable")
    else HTTP 404
        API-->>Provider: 404
        Provider-->>Adapter: NotFound
        Adapter->>Adapter: beginCandidateSearch()<br/>降级 / degrade
    else HTTP 200 但 JSON 无效 / invalid JSON
        API-->>Provider: 200 + 损坏载荷 / malformed body
        Provider->>Provider: parseRecord 失败 / fails
        Provider-->>Adapter: ProviderError("provider-failed")
        Adapter-->>Ctrl: failed("provider-failed")
    end

    Ctrl->>Ctrl: stableErrorCode(errorCode)
    Note over Ctrl: allowlist 过滤 / filter:<br/>network-unavailable, rate-limited,<br/>database-failed, provider-failed<br/>其余归一为 provider-failed
    Ctrl->>Ctrl: setStatus(Error, code)
```

**中文**

`stableErrorCode` 的 allowlist（[lyricsservicecontroller.cpp:56-65](../daemon/lyrics-dockd/application/lyricsservicecontroller.cpp#L56-L65)）确保对外暴露的错误码是有限枚举，不泄漏服务端原始响应。未在清单内的错误码统一归一为 `provider-failed`。

注意 `track-not-searchable` 与 `invalid-candidate` 不在 allowlist 内，会被归一为 `provider-failed`——这使设置应用中针对前者的专门文案（[settingswindow.cpp:536](../apps/lyrics-settings/settingswindow.cpp#L536)）在此路径下无法触达。该文案仅在设置应用直接调用失败时可见。

**English**

The `stableErrorCode` allowlist ([lyricsservicecontroller.cpp:56-65](../daemon/lyrics-dockd/application/lyricsservicecontroller.cpp#L56-L65)) keeps externally visible error codes to a bounded enumeration and never leaks raw server responses. Codes outside the list collapse to `provider-failed`.

Note that `track-not-searchable` and `invalid-candidate` are absent from the allowlist and collapse to `provider-failed`, which means the dedicated message for the former ([settingswindow.cpp:536](../apps/lyrics-settings/settingswindow.cpp#L536)) is unreachable along this path. That message appears only when a direct settings-app call fails.

## 6. 请求代次守卫 / Request Generation Guard

```mermaid
sequenceDiagram
    participant Ctrl as ServiceController
    participant Adapter as LrclibAdapter
    participant Provider as LRCLIBProvider

    Ctrl->>Adapter: search(曲目 A / track A)
    Adapter->>Adapter: generation = ++m_generation （= 1）
    Adapter->>Provider: getExact(A) 异步 / async

    Note over Ctrl: 用户快速切歌 / user skips quickly
    Ctrl->>Adapter: search(曲目 B / track B)
    Adapter->>Adapter: generation = ++m_generation （= 2）
    Adapter->>Provider: getExact(B) 异步 / async

    Provider-->>Adapter: A 的响应到达 / A's response arrives
    Adapter->>Adapter: isCurrent(1)？1 ≠ 2 → false
    Note over Adapter: 丢弃过期响应<br/>discard stale response

    Provider-->>Adapter: B 的响应到达 / B's response arrives
    Adapter->>Adapter: isCurrent(2)？2 == 2 → true
    Adapter-->>Ctrl: lyricsReady(B 的载荷 / B's payload)
```

**中文**

`m_generation` 单调递增，每次新检索递增一次。回调中通过 `isCurrent(generation)` 判断响应是否仍然相关，过期响应静默丢弃。这防止快速切歌时旧响应覆盖新歌词。

同一机制也用于 `clearCache()`（[lrcliblyricsadapter.cpp:87](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L87)）——清缓存时递增代次，使所有在途响应失效。

**English**

`m_generation` increases monotonically, once per new lookup. Callbacks check `isCurrent(generation)` to decide whether a response is still relevant, silently dropping stale ones. This prevents an older response from overwriting newer lyrics during rapid skipping.

The same mechanism serves `clearCache()` ([lrcliblyricsadapter.cpp:87](../daemon/lyrics-dockd/infrastructure/lrcliblyricsadapter.cpp#L87)): bumping the generation invalidates every in-flight response.
