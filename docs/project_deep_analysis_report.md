# 项目深度分析报告

> 审查日期：2026-04-26  
> 审查范围：当前工作区 Qt/C++ 源码、资源、构建配置、文档与测试资料。已按要求忽略 `build/`、`build-*`、`debug/`、`release/`、`.git/`、`.vscode/`、`Makefile*`、`*.user`、`*.pro.user`、编译产物和第三方缓存文件。  
> 审查方式：静态代码阅读、结构扫描和关键风险点检索。未修改业务代码，未删除文件，未执行构建或自动化测试。

## 1. 项目概览

- 项目类型：桌面端 Qt/C++ 影音播放器应用。
- 技术栈：Qt 6 Widgets、Qt Multimedia、Qt MultimediaWidgets、Qt Network、Qt SQL、Qt Concurrent、qmake、C++17、MySQL/QMYSQL、可选 FFmpeg、DashScope/OpenAI 兼容 AI 接口。
- 主要功能：
  - 本地视频播放、进度恢复、倍速、音量、截图、无声画面录制。
  - 本地音频播放、播放列表、播放模式、在线音乐搜索、歌词同步、模拟频谱。
  - 播放历史统一展示、音频/视频历史回放、历史删除和清空。
  - MySQL 用户注册、登录、退出、修改密码、登录次数、弹幕计数。
  - Bilibili 视频搜索、直链解析、浏览器兜底打开。
  - 网易云音乐搜索、播放前解析真实音频 URL。
  - LRC 本地解析、在线歌词加载和缓存。
  - 视频弹幕输入、展示、列表同步、我的弹幕记录。
  - AI 影片助手，支持截取当前视频画面并调用视觉语言模型。
- 当前整体架构：
  - 启动层：`main.cpp` 创建 `QApplication`，交给 `AppBootstrapper` 加载主题和初始化数据库，再将 `AppStartupState` 传入 `Widget`。
  - 主窗口层：`Widget` 负责顶层窗口、菜单和状态提示；`MainWindowController` 负责页面切换、本地媒体路由和历史回放；`MediaPlaybackRouter` 负责按历史记录类型分派到音频或视频播放器。
  - 播放层：音频由 `AudioPlayer + AudioPlayerController + AudioPlaybackController + PlaylistModel` 组织；视频由 `VideoPlayerWidget + VideoPlayerController + VideoPlaybackController` 组织，并继续拆出历史、截图录制、弹幕和在线视频 coordinator。
  - 数据层：`DatabaseManager` 是全局 QMYSQL 连接与迁移入口，`DatabaseContext` 为 repository 提供 provider，`UserRepository`、`MediaHistoryRepository`、`DanmakuRepository` 承担主要 SQL。
  - 网络层：`NetworkClient` 提供异步 GET/POST、超时、重试、取消、错误模型和日志策略；在线音乐、在线视频、歌词和 AI 服务都基于它。
  - UI 与资源层：`resources/resources.qrc` 打包图标和 `resources/styles/app.qss`，`UiTheme` 加载全局 QSS；多个界面仍有内联样式。
- 代码质量总体评价：
  - 优点：项目已经明显从早期大类向 controller/service/repository/coordinator 方向演进；`AppBootstrapper`、`MainWindowController`、`MediaPlaybackRouter`、`NetworkClient`、`BilibiliSearchService`、`BilibiliPlaybackResolver` 等边界比旧报告中更清晰；在线视频解析已避免同步 `QEventLoop`；弹幕同步已由播放器位置驱动；在线音频历史可保存结构化 query 信息并在播放前重新 resolve。
  - 主要风险：根目录仍过度拥挤，UI 组装和业务决策仍在多个类中混合；`VideoPlayerWidget` 是 `QObject` facade 却命名为 Widget；历史表唯一键仍基于 `file_path`；数据库强依赖 QMYSQL/MySQL；遗留弹幕头文件和 `LyricDownloader` 仍保留；自动化测试几乎缺失。
  - 工程化结论：当前代码比普通课程原型更接近可维护架构，但还没有形成稳定交付所需的模块边界、测试体系和部署规范。后续重构应优先收敛历史/弹幕/样式/认证边界，再补自动化测试。

## 2. 目录结构分析

### 根目录源码

根目录包含 `main.cpp`、`widget.*`、`audioplayer.*`、`videoplayer.*`、`mediahistory.*`、`dbmanager.*`、`authservice.*`、`lyricservice.*`、`aichat*`、`playlist*`、`video*coordinator.*` 等大量文件。它们承担了应用入口、主窗口、音视频播放、认证、历史、歌词、AI、构建和若干兼容层职责。

结构问题是根目录职责仍然太宽。虽然内部类已经按 `Controller/Service/Repository/Coordinator` 命名拆分，但文件物理位置没有跟上，阅读者很难从目录直接判断模块归属。建议后续迁移为：

```text
src/app/
src/audio/
src/video/
src/history/
src/auth/
src/database/
src/network/
src/lyrics/
src/danmaku/
src/ai/
src/ui/
```

### `danmaku/`

`danmaku/` 是较新的弹幕模块，包含 `DanmakuController`、`DanmakuInputBar`、`DanmakuOverlay`、`DanmakuPanel`、`DanmakuRepository`、`DanmakuItem`。当前 `.pro` 已只编译这些新类，根目录旧弹幕类没有进入 `.pro`。

结构总体合理，但旧文件 `danmakumanager.*`、`danmakuinput.h`、`danmakudisplay.h`、`danmakuwidget.h` 仍留在根目录。它们不再参与构建，却仍会被搜索和补全命中，建议移动到 `tools/legacy/` 或删除。

### `network/`

`network/` 包含 `NetworkClient`、`OnlineMusicService`、`OnlineVideoService`、`LyricDownloadService`、`AiChatService`、`BilibiliSearchService`、`BilibiliPlaybackResolver`、`onlinevideotypes.h`。这是目前目录结构中最清晰的部分：基础网络能力、第三方 API 封装和业务服务集中在一个子目录。

仍需改进的是命名和兼容信号：`OnlineMusicService` 同时保留 `searchMusic()`、`search()`、`searchSongsAsync()` 和 `searchFinished(QStringList)`，这让新旧 API 边界不够干净。

### `resources/` 与 `assets/`

`resources/resources.qrc` 将 `assets/*.png` 和 `resources/styles/app.qss` 打包到 Qt 资源系统，运行时通过 `:/assets/...` 和 `:/styles/app.qss` 访问，方向正确。`UiTheme` 已集中加载全局样式。

不足是 `resources.qrc` 使用 `../assets/...`，资产目录和 qrc 目录分离，移动目录时容易漏改。样式来源也较分散：`app.qss`、`AudioStyle` 属性封装、以及多个 UI 类中的 `setStyleSheet()` 并存。

### `tests/`

当前只有 `tests/audio_manual_verification.md`，是手工验证清单。它已覆盖播放列表、音频控制、歌词和在线音乐手工场景，但没有 QTest、没有数据库测试、没有网络解析测试、没有迁移测试，也没有 CI 入口。

### `docs/`

`docs/project_deep_analysis_report.md` 是本报告。`docs/legacy_inventory.md` 已记录根目录旧弹幕文件的删除条件，这是很好的重构过程文档。`docs/generated/ai-model-hub.html` 更像生成产物，与 QtMediaPlayer 主项目关系弱，建议加入 `.gitignore` 或迁移到归档区。

### `tools/legacy/`

`tools/legacy/gen_aichat.py` 已加显式保护参数，默认拒绝运行，这是比旧状态安全的。问题是脚本仍硬编码 `D:/MyQtProject/QtMediaPlayer/aichatwidget.h` 并具备追加源码的副作用，长期仍应移出生产仓库或改成只读归档。

### 构建与本机产物

`.gitignore` 已覆盖 `database.ini`、`*.dll`、`*.lib`、`bin/`、`obj/`、`moc/`、`rcc/`、`ui/`、`build/`、`build-*`、`Makefile*` 等常见产物。本地工作区仍能看到 `database.ini`、`libmysql.dll`、`libmysql.lib`、`bin/`、`obj/`、`moc/`、`rcc/`、`ui/` 等文件，它们应继续保持未跟踪。

目录结构结论：源码逻辑已经开始模块化，但文件布局仍停留在“根目录集中式”阶段。下一步最有价值的目录改造是把已经清晰的模块搬到物理子目录，而不是继续在根目录增加文件。

## 3. 模块划分分析

### 应用启动模块

#### 相关文件

- `main.cpp`
- `appbootstrapper.h`
- `appbootstrapper.cpp`
- `appstartupstate.h`
- `uitheme.h`
- `uitheme.cpp`

#### 当前职责

`main.cpp` 只负责创建 `QApplication`、调用 `AppBootstrapper::initialize()`、创建 `Widget` 并进入事件循环。`AppBootstrapper` 设置组织名和应用名，加载 `UiTheme("app")`，初始化 `DatabaseManager`，并把主题/数据库状态写入 `AppStartupState`。

#### 主要问题

- `AppStartupState` 只有 `themeLoaded/databaseAvailable` 和错误文本，无法表达“数据库不可用时哪些功能应降级”。
- 数据库初始化失败时应用继续启动，这是合理的降级策略，但具体功能开关没有集中管理。`AuthService`、历史、弹幕会在各自操作时再次失败。
- `AppBootstrapper` 当前只初始化主题和数据库，`UserSession`、`AuthService`、`MediaHistoryService` 等仍由 `Widget` 创建，应用级依赖装配尚未集中。
- `UiTheme` 只加载全局 QSS，但全局样式和模块内联样式之间没有约束。

#### 修改建议

- 将 `AppStartupState` 扩展为能力状态，而不是只记录错误文本：

```cpp
struct AppStartupState {
    bool themeLoaded = false;
    bool databaseAvailable = false;
    bool authAvailable = false;
    bool historyAvailable = false;
    bool danmakuAvailable = false;
    QStringList warnings;
};
```

- 新增 `AppServices` 或 `ApplicationContext`，由 `AppBootstrapper` 创建和持有核心服务，再传给 `Widget`。这样 `Widget` 不需要知道数据库和认证服务如何构造。
- 数据库不可用时，UI 应禁用登录、历史、弹幕相关入口，或至少在按钮点击前给出一致提示。
- 为 `UiTheme` 增加样式加载失败后的显式状态展示，避免只依赖 `qWarning()`。

#### 修改优先级

中优先级。当前启动路径已经比较清爽，但服务装配仍散在主窗口，后续模块测试和降级逻辑都会受影响。

### 主窗口与页面协调模块

#### 相关文件

- `widget.h`
- `widget.cpp`
- `widget.ui`
- `mainwindowcontroller.h`
- `mainwindowcontroller.cpp`
- `mediaplaybackrouter.h`
- `mediaplaybackrouter.cpp`
- `menu.h`
- `menu.cpp`

#### 当前职责

`Widget` 是顶层窗口，加载 `widget.ui` 中的 `QStackedWidget`，创建共享 `UserSession`、`AuthService`、`AuthDialogController`、`MediaHistoryService`，再创建视频页和音频页。`MainWindowController` 负责切换视频/音频页面、打开本地媒体并按扩展名路由、历史回放。`MediaPlaybackRouter` 根据 `MediaHistoryRecord.fileType` 分发给音频或视频播放器。

#### 主要问题

- `Widget` 仍承担服务创建、菜单创建、启动状态提示、关于弹窗、页面对象创建等职责，顶层类偏重。
- `MainWindowController::openLocalMediaFiles()` 已能按类型路由，这是好改进；但支持格式列表在方法内硬编码，音频/视频模块各自也有格式假设，未来容易不一致。
- `MainWindowController::showVideoPage()` 和 `showAudioPage()` 在页面切换时直接暂停另一播放器，策略写死在主窗口层，缺少可配置的播放互斥策略。
- `MediaPlaybackRouter` 对视频历史只调用 `openAtPosition()`，没有检查本地文件是否存在；文件不存在时依赖后续播放器错误。
- `widget.ui` 只提供两个空 page 和少量背景样式，实际布局都在 C++ 里创建，`.ui` 的价值有限。
- `Menu::createActionGroup(..., true)` 会把文件菜单项也设为 checkable，例如“打开媒体”“播放历史”“退出”，这类命令型 action 不应该是可选状态。

#### 修改建议

- 将 `Widget` 收敛为纯窗口壳，服务创建移入 `AppServices`：

```cpp
struct AppServices {
    UserSession* userSession = nullptr;
    AuthService* authService = nullptr;
    MediaHistoryService* historyService = nullptr;
};
```

- 将媒体格式列表提取为 `MediaTypeDetector` 或静态配置，供主窗口、文件对话框、音频和视频模块共同使用。
- `MediaPlaybackRouter` 播放视频历史前增加本地路径存在性检查，并为在线/远程视频历史预留分支。
- `Menu` 支持区分 command action 和 exclusive/checkable action，文件菜单不要默认 checkable。
- 如果继续保留 `.ui`，应把主布局交给 `.ui`；如果主布局全部由 C++ 构建，则可以考虑删除 `.ui`，改成纯代码主窗口。

#### 修改优先级

高优先级。主窗口是所有模块的连接点，服务装配、媒体路由和菜单行为会直接影响后续重构成本。

### 音频播放模块

#### 相关文件

- `audioplayer.h`
- `audioplayer.cpp`
- `audioplayercontroller.h`
- `audioplayercontroller.cpp`
- `audioplaybackcontroller.h`
- `audioplaybackcontroller.cpp`
- `audioplayerwidget.h`
- `audioplayerwidget.cpp`
- `audiocontrolbar.h`
- `audiocontrolbar.cpp`
- `audiodialogservice.h`
- `audiodialogservice.cpp`
- `playlistmodel.h`
- `playlistmodel.cpp`
- `playlistpanel.h`
- `playlistpanel.cpp`
- `audiotrack.h`
- `spectrumpanel.h`
- `spectrumpanel.cpp`
- `spectrumwidget.h`

#### 当前职责

音频模块负责本地音频添加、播放列表维护、播放控制、音量/进度/模式、在线音乐加入、播放前解析真实 URL、播放失败状态展示、歌词联动、模拟频谱、音频历史保存和历史恢复。

#### 主要问题

- `AudioPlayer` 仍是较重的页面总控：创建 UI、创建服务、连接信号、处理文件选择、登录入口、诊断弹窗和专辑封面。
- `AudioPlayerController` 达到 600 多行，集中了播放列表操作、在线 URL resolve、歌词同步、历史 key 编码、错误映射和诊断文本，职责偏多。
- `OnlineMusicService` 的 `searchFinished(QStringList)` 仍作为 legacy 信号保留，`AudioPlayerController` 使用的是结构化 `SongInfo`，旧信号会增加误用可能。
- 在线音频历史通过 `online-audio:` 后接 URL query 保存，能工作，但这是把结构化数据塞进 `file_path` 字段的过渡方案。标题、艺术家、source_id、url、lyric_url 应成为历史表结构化列。
- `PlaylistModel` 是普通 `QObject`，`PlaylistPanel::refreshFromModel()` 每次整表重建。当前歌曲数量小可接受，但不适合大量曲目和局部状态更新。
- `SpectrumWidget` 仍是 230 行头文件实现，且注释明确是模拟频谱，不是真实音频数据。它还包含 `QAudioSink` 但未使用。
- `testAudio()` 诊断功能出现在普通播放列表面板中，发布版用户可能不需要。

#### 修改建议

- 将 `AudioPlayerController` 拆成三个对象：`PlaylistController`、`OnlineAudioResolver`、`AudioHistoryRecorder`。`AudioPlayerController` 只协调这些对象。
- 把历史 key 编码逻辑从 `AudioPlayerController::historyKeyForTrack()` 抽成 `AudioHistoryIdentity`，后续数据库迁移时只改一个地方。
- 将 `PlaylistModel` 改为 `QAbstractListModel`，`PlaylistPanel` 使用 model/view 数据角色展示标题、状态、tooltip 和颜色。
- 将 `SpectrumWidget` 移到 `.cpp`，并重命名为 `SimulatedSpectrumWidget`，或者接入真实音频采样后再保留频谱名称。
- 隐藏或移动 `testAudio()` 到开发菜单，例如通过 `QT_DEBUG` 或设置开关控制。

#### 修改优先级

中优先级。音频链路已经能表达在线解析失败和历史恢复，但 controller 过大、历史字段过渡和模拟频谱会影响长期维护。

### 视频播放模块

#### 相关文件

- `videoplayer.h`
- `videoplayer.cpp`
- `videoplayercontroller.h`
- `videoplayercontroller.cpp`
- `videoplaybackcontroller.h`
- `videoplaybackcontroller.cpp`
- `videocontrolbar.h`
- `videocontrolbar.cpp`
- `videocapturecoordinator.h`
- `videocapturecoordinator.cpp`
- `captureservice.h`
- `captureservice.cpp`
- `framecaptureservice.h`
- `framecaptureservice.cpp`
- `videocapture.h`
- `videocapture.cpp`
- `videoencoder.h`
- `videoencoder.cpp`

#### 当前职责

视频模块负责本地视频播放、在线视频播放、进度保存和恢复、控制栏、弹幕协调、截图、无声画面录制、AI 面板挂载和播放错误提示。

#### 主要问题

- `VideoPlayerWidget` 实际继承 `QObject`，不是 `QWidget`，文件中已有 `TODO(video-facade-rename)`。这个命名会持续误导维护者。
- `VideoPlayerWidget::createLayout()` 一次性创建视频容器、弹幕列表、AI 面板、控制栏和输入栏，UI 组装仍偏重。
- `VideoPlayerController` 已拆出 `VideoHistoryCoordinator`、`VideoCaptureCoordinator`、`VideoDanmakuCoordinator`、`OnlineVideoCoordinator`，方向正确，但仍有 pending seek、播放错误、在线解析结果处理、保存进度等多类职责。
- `VideoControlBar` 对象名是 `controlPannel`，拼写错误；成员 `m_btnCtr` 命名不清晰。控制按钮大量使用 `Search/Hist/Shot/REC/Login/DM/Mine` 文本而不是图标，界面国际化和可读性较弱。
- `FrameCaptureService` 仍依赖 `screen->grabWindow(0, ...)` 截屏，虽然有 `widget->grab()` 兜底，但在高 DPI、多屏、Wayland、远程桌面或视频硬件叠加场景下可能截不到真实帧。
- 无声画面录制是每 200ms 截一张 PNG，再以 5fps 转 MP4。当前 UI 文案已经说明“不包含音频”，但实现本质仍是截图序列录制，帧率和画质固定。
- `VideoEncoder::findFFmpeg()` 已支持 `FFMPEG_PATH` 和 PATH 查找，但仍有 Windows 绝对路径候选，部署策略需要文档化。

#### 修改建议

- 将 `VideoPlayerWidget` 重命名为 `VideoPlayerFacade`，同步更新 `Widget`、`MainWindowController`、`MediaPlaybackRouter` 和 using alias。
- 拆出 `VideoPageLayoutBuilder` 或真正的 `VideoPlayerPage : QWidget`，让 facade 只负责连接服务。
- 将 pending seek 恢复逻辑独立成 `VideoResumeController`：

```cpp
struct PendingSeek {
    qint64 position = -1;
    bool resumeMode = false;
};
```

- 将 `FrameCaptureService` 作为统一截图接口保留，但在文档中明确它是“窗口截图”，不是底层视频帧获取。长期可评估 `QVideoSink` 或平台相关帧捕获。
- 控制栏按钮改为 qrc 图标加 tooltip，修正 `controlPannel` 和 `m_btnCtr` 命名。
- 录制参数抽为配置：帧间隔、fps、质量、保存目录、是否保留 PNG 帧。

#### 修改优先级

高优先级。视频 facade 命名和截图录制方式是维护和用户预期的关键风险。

### 播放历史模块

#### 相关文件

- `mediahistory.h`
- `mediahistory.cpp`
- `unifiedhistorydialog.h`
- `unifiedhistorydialog.cpp`
- `videohistorycoordinator.h`
- `videohistorycoordinator.cpp`
- `mediaplaybackrouter.h`
- `mediaplaybackrouter.cpp`
- `migrationrunner.h`
- `migrationrunner.cpp`

#### 当前职责

`MediaHistoryRepository` 和 `MediaHistoryService` 是统一历史入口，负责 `play_history` 的保存、进度更新、完成状态、查询、删除和清空。`UnifiedHistoryDialog` 展示全部/音频/视频历史。`VideoHistoryCoordinator` 负责视频历史对话框和恢复进度提示。`MediaPlaybackRouter` 负责从历史记录回到音频或视频播放。

#### 主要问题

- 当前非忽略源码中已没有 `playhistory.*`、`videohistory.*`、`videohistoryservice.*`、`videohistorydialog.h`，历史模块已基本收敛，这是好状态。但 `moc/`、`obj/` 等忽略产物里仍残留旧名字，容易在手工搜索时干扰判断。
- `play_history.file_path` 在迁移版本 1 中仍是 `VARCHAR(768) UNIQUE`。`mediahistory.cpp` 也有 TODO，说明理想唯一键应是 `(file_type, source_type, source_id)`。
- `MediaHistoryRepository::saveRecord()` 更新已有记录时最终 `UPDATE ... WHERE file_path = :file_path`，如果未来允许同一路径多类型或多来源，会直接覆盖。
- 在线音频历史已经把 source、source_id、url、title、artist、album、lyric_url 编码进 `online-audio:` query，但数据库层仍无法结构化查询这些字段。
- `trimHistory()` 对整个表裁剪，不按媒体类型裁剪。音频播放量大时可能挤掉视频历史。
- `VideoHistoryCoordinator::restoreProgressIfNeeded()` 直接弹 `QMessageBox`，服务/协调层混入 UI 决策。
- `UnifiedHistoryDialog` 在 `playSelected()` 中同时发 `playRequested(record)` 和旧的 `playAudio/playVideo` 信号，存在重复信号路径。

#### 修改建议

- 迁移 `play_history` 到新 schema，增加 `source_type`、`source_id`、`source_url`、`title`、`artist`、`thumbnail` 等列，唯一键调整为 `(file_type, source_type, source_id)`。
- 将 `online-audio:` query 仅作为兼容读取逻辑，新写入走结构化列。
- `trimHistory()` 支持按 `file_type` 裁剪，或者保留总量和单类型上限两个配置。
- `VideoHistoryCoordinator` 只返回恢复候选，弹窗放到 UI 层：

```cpp
struct ResumeCandidate {
    qint64 position = 0;
    qint64 duration = 0;
    int progressPercent = 0;
};
```

- 删除 `UnifiedHistoryDialog` 的 `playAudio/playVideo` 旧信号，统一使用 `playRequested(const MediaHistoryRecord&)`。

#### 修改优先级

高优先级。历史模块已经完成表层合并，下一步应解决数据库身份模型，否则在线媒体和跨类型历史会继续绕路。

### 数据库与认证模块

#### 相关文件

- `dbmanager.h`
- `dbmanager.cpp`
- `databasecontext.h`
- `databasecontext.cpp`
- `databaseconfigloader.h`
- `databaseconfigloader.cpp`
- `migrationrunner.h`
- `migrationrunner.cpp`
- `userrepository.h`
- `userrepository.cpp`
- `authservice.h`
- `authservice.cpp`
- `authdialogcontroller.h`
- `authdialogcontroller.cpp`
- `logindialog.h`
- `logindialog.cpp`
- `usersession.h`
- `usersession.cpp`
- `database.ini.example`

#### 当前职责

`DatabaseManager` 管理 QMYSQL 连接、自动建库和 schema 迁移。`DatabaseConfigLoader` 从应用目录、环境变量和用户配置目录读取配置。`UserRepository` 管理用户表 SQL、密码散列和登录信息。`UserAccountService` 是认证账号服务兼容层。`AuthService` 管理会话、登录、注册、修改密码和弹幕计数 provider。`AuthDialogController` 和 `LoginDialog` 管理认证 UI。

#### 主要问题

- `DatabaseManager` 当前明确拒绝非 QMYSQL driver，跨机器部署高度依赖 MySQL 服务、QMYSQL plugin、`libmysql.dll` 和 `database.ini`。
- `DatabaseConfigLoader::configPath()` 查找顺序是应用目录、`QTMEDIAPLAYER_DB_CONFIG`、用户 AppConfig、GenericConfig。应用目录优先可能导致发布包旁边的旧 `database.ini` 覆盖用户配置。
- `UserAccountService` 和 `DBManager` deprecated alias 仍保留，且 `dbmanager.h` 同时定义全局数据库管理器和用户账号服务，文件职责偏宽。
- `UserRepository` 已使用 salted SHA-256 并支持旧 SHA-256 自动升级，但 TODO 明确指出应迁移到 PBKDF2/Argon2/bcrypt/scrypt。当前方案不适合高安全要求。
- `AuthService::initialize()` 数据库失败时使用 `qDebug()` 输出错误，日志级别和分类不统一。
- `AuthDialogController::showChangePasswordDialog()` 大量 UI 构建和样式写在 controller 中，UI/业务混杂。
- `LoginDialog` 只实现“记住用户名”，没有记住密码，这是安全上更好的方向，但需要确保 UI 文案一直保持“记住用户名”，避免误解。

#### 修改建议

- `DatabaseConfigLoader` 调整优先级：显式环境变量路径优先，其次用户配置目录，最后应用目录。
- 将 `UserAccountService` 移到 `auth` 模块，例如 `useraccountservice.*`，`dbmanager.*` 只保留数据库连接。
- 增加 `DatabaseAvailability` 或能力开关对象，认证/历史/弹幕 UI 根据该对象启用或禁用入口。
- 密码散列迁移到专用 KDF，保留旧算法按登录时升级：

```cpp
enum class PasswordAlgorithm {
    LegacySha256,
    SaltedSha256,
    Pbkdf2Sha256
};
```

- `AuthDialogController` 只协调 `LoginDialog` 和 `ChangePasswordDialog`，把修改密码 UI 独立成类。
- 使用 `QLoggingCategory` 替代 `AuthService` 中的 `qDebug()`。

#### 修改优先级

高优先级。数据库和认证是部署、登录和弹幕/历史的基础，任何隐式失败都会影响多个模块。

### 网络请求模块

#### 相关文件

- `network/networkclient.h`
- `network/networkclient.cpp`

#### 当前职责

`NetworkClient` 封装 `QNetworkAccessManager`，提供异步 GET/POST JSON、请求 ID、超时、取消、重试、HTTP/Qt 错误归一、`ServiceError`、敏感参数脱敏日志和 `QLoggingCategory`。

#### 主要问题

- `NetworkClient` 本身较完整，但没有自动化测试覆盖超时、取消、重试、HTTP 4xx/5xx 和日志脱敏。
- `RequestOptions` 同时有 `timeout` 和 `timeoutMs`，且 `timeoutMs >= 0` 优先，容易让调用者混淆。
- `retry` 是最大重试次数，但命名不像 `maxRetries` 那么明确。
- 重试退避是 `400 * attempt` 的固定线性延迟，没有抖动，也没有针对 429 的 `Retry-After` 支持。
- `NetworkClient` 创建真实 `QNetworkAccessManager`，目前不易注入 fake network backend 做单元测试。

#### 修改建议

- 精简 `RequestOptions`，保留 `timeoutMs` 和 `maxRetries`，旧字段标记 deprecated。
- 为网络客户端抽接口或允许注入 `QNetworkAccessManager`，便于 QTest 使用本地 fake reply。
- 对 429 和 503 支持 `Retry-After`，并加入最大退避时间。
- 增加测试：成功响应、HTTP 404、HTTP 500 重试、超时 abort、主动 cancel、敏感 query/header 脱敏。

#### 修改优先级

中优先级。当前网络层已经比多数业务层更规范，风险主要在缺少测试和 API 命名兼容。

### 在线音乐模块

#### 相关文件

- `network/onlinemusicservice.h`
- `network/onlinemusicservice.cpp`
- `onlinemusicsearch.h`
- `onlinemusicsearch.cpp`
- `audioplayercontroller.cpp`

#### 当前职责

在线音乐模块使用网易云公开接口搜索歌曲，展示候选结果；用户加入播放列表后并不立即信任 URL，而是在真正播放前调用 `resolveSongUrlAsync()` 获取真实播放地址，并把播放状态写回列表项。

#### 主要问题

- 依赖 `music.163.com/api/...` 和第三方行为，接口稳定性不可控，没有 mock/fallback 数据，也没有测试样本。
- `OnlineMusicService` 同时负责创建 `OnlineMusicSearch` 对话框，服务层和 UI 层耦合。
- 保留 `searchFinished(QStringList)`、`searchMusic()`、`search()` 等旧入口，容易让新代码误用非结构化结果。
- `OnlineMusicSearch` 直接持有 service 并弹窗，适合当前项目，但不利于把搜索组件复用到其他 UI。
- 搜索请求没有输入防抖；用户连续点击搜索时只记录一个 `m_pendingSearchRequestId`，旧请求返回会被忽略，但没有主动 cancel。

#### 修改建议

- 拆成 `OnlineMusicSearchService`、`OnlineMusicResolver`、`OnlineMusicSearchDialog`，服务层不创建对话框。
- 删除或标记旧 `QStringList` 信号，统一使用 `QList<SongInfo>`。
- 搜索前取消上一个未完成搜索请求，或者为 UI 提供请求状态队列。
- 添加固定 JSON 样本测试，覆盖空结果、接口错误、缺少 URL、URL 非 http/https。
- 长期将在线音乐历史结构化保存到 `play_history` 新列，而不是编码在 `file_path`。

#### 修改优先级

中优先级。播放前 resolve 的核心风险已降低，但服务/UI耦合和旧 API 会影响后续扩展。

### 在线视频模块

#### 相关文件

- `network/onlinevideoservice.h`
- `network/onlinevideoservice.cpp`
- `network/bilibilisearchservice.h`
- `network/bilibilisearchservice.cpp`
- `network/bilibiliplaybackresolver.h`
- `network/bilibiliplaybackresolver.cpp`
- `network/onlinevideotypes.h`
- `onlinevideosearch.h`
- `onlinevideosearch.cpp`
- `onlinevideocoordinator.h`
- `onlinevideocoordinator.cpp`

#### 当前职责

在线视频模块负责 Bilibili 关键词搜索、结果展示、网页打开、播放直链解析、DASH 探测和浏览器兜底。`OnlineVideoService` 现在只是 search service 和 playback resolver 的门面。

#### 主要问题

- 当前代码已经不再使用同步 `QEventLoop` 或 deprecated 同步 API，这是明显改进。
- `BilibiliPlaybackResolver` 仍有 400 多行，承担 bvid 提取、cid 解析、playurl 请求、单流解析、DASH 探测、浏览器兜底文案，类偏大。
- 对 Bilibili 未登录、版权、风控、DASH 音视频分离等限制处理较谨慎，但仍依赖公开接口行为，容易随平台变化失效。
- `OnlineVideoSearch::onPlaySelected()` 会显示“已发送到播放器”，但真正是否可播放要等 resolver 返回；文案可能被用户理解为播放成功。
- 在线视频播放没有进入统一播放历史，因为 `VideoPlayerController::clearLocalVideoStateForOnlinePlayback()` 会清空本地路径，历史模块主要服务本地视频。

#### 修改建议

- 将 `BilibiliPlaybackResolver` 继续拆分为 `BilibiliUrlFactory`、`BilibiliResponseParser`、`BilibiliPlaybackResolver`。
- 将“已发送到播放器”改为“正在解析播放地址”，resolver 成功后再显示“开始播放”。
- 为搜索响应、pagelist 响应、单流 durl、DASH-only、接口错误准备 JSON 样本测试。
- 给在线视频历史设计 `source_type=bilibili`、`source_id=bvid/cid`、`page_url`、`media_url` 等结构化字段。
- 如要支持 DASH 直播，单独引入可合流或多轨播放能力，不要把 DASH video-only URL 交给 `QMediaPlayer` 伪装成功。

#### 修改优先级

中优先级。当前失败处理已经比较诚实，主要风险是平台接口变动和缺少测试。

### 歌词模块

#### 相关文件

- `lyricparser.h`
- `lyricparser.cpp`
- `lyricservice.h`
- `lyricservice.cpp`
- `lyricwidget.h`
- `lyricwidget.cpp`
- `lyricpanel.h`
- `lyricpanel.cpp`
- `network/lyricdownloadservice.h`
- `network/lyricdownloadservice.cpp`
- `lyricdownloader.h`

#### 当前职责

歌词模块负责 LRC 文本解析、本地歌词查找、AppData 歌词缓存、在线歌词请求、歌词状态展示和播放进度联动。`LyricService` 是当前主要入口，`LyricDownloader` 是 deprecated 兼容包装。

#### 主要问题

- `LyricParser` 已移到 `.cpp`，这是好改进；但仍有多处 `qDebug()` 默认输出，如找到歌词、未找到歌词、创建示例歌词。
- `LyricDownloader` 在头文件中完整实现并仍列在 `.pro` 的 HEADERS 中，属于兼容残留。
- `LyricDownloadService` 使用第三方 Vercel 网易云 API，稳定性不可控，没有测试样本。
- `LyricService::handleOnlineLyricResponse()` 在线歌词解析为空时直接显示解析失败，没有继续回退本地/下载。网络失败会回退，但“内容为空”不会回退。
- `LyricParser::createSampleLyric()` 会写到音频同目录，若音频在只读目录会失败；虽然它可能不是主链路，但仍是路径权限风险。
- `LyricWidget`/`LyricPanel` 的 UI 细节较多，样式和状态文本分散在代码中。

#### 修改建议

- 删除或移动 `LyricDownloader`，保留一版迁移说明即可。
- 使用 `QLoggingCategory` 管理歌词解析日志，默认不输出“未找到歌词”这类调试信息。
- 在线歌词内容为空时也尝试本地/下载兜底，或明确区分“接口返回无歌词”和“解析失败”。
- 为 `LyricParser::parseLrcText()` 增加 QTest，覆盖多时间标签、无毫秒、元信息标签、空行、乱序排序。
- 将示例歌词写入 AppData，而不是音频同目录。

#### 修改优先级

中优先级。歌词核心功能清晰，但 deprecated 包装、日志和第三方接口测试需要整理。

### 弹幕模块

#### 相关文件

- `danmaku/danmakucontroller.h`
- `danmaku/danmakucontroller.cpp`
- `danmaku/danmakuinputbar.h`
- `danmaku/danmakuinputbar.cpp`
- `danmaku/danmakuoverlay.h`
- `danmaku/danmakuoverlay.cpp`
- `danmaku/danmakupanel.h`
- `danmaku/danmakupanel.cpp`
- `danmaku/danmakurepository.h`
- `danmaku/danmakurepository.cpp`
- `danmaku/danmakuitem.h`
- `danmaku/danmakuitem.cpp`
- `videodanmakucoordinator.h`
- `videodanmakucoordinator.cpp`
- `mydanmakudialog.h`
- `mydanmakudialog.cpp`
- `danmakumanager.*`
- `danmakuinput.h`
- `danmakudisplay.h`
- `danmakuwidget.h`

#### 当前职责

弹幕模块负责本地视频弹幕的数据库读写、媒体 ID 生成、旧路径数据迁移、弹幕发送、覆盖层绘制、列表展示、登录检查、播放位置同步和用户弹幕记录。

#### 主要问题

- 新弹幕模块已独立实现，不再继承旧头文件类，这是好改进。旧根目录文件仍保留但不在 `.pro` 中编译。
- `DanmakuController::mediaIdForVideo()` 使用 canonical path、文件大小、mtime 生成 hash。文件移动会因 canonical path 变化导致新的 media_id，旧弹幕依赖 `video_path` fallback 才能迁移；如果文件移动后路径也变了，弹幕仍会断关联。
- `DanmakuRepository::migrateLegacyRows()` 在多次读取时可能反复尝试更新旧行，缺少显式迁移完成标记。
- `DanmakuOverlay` 使用 30ms 定时器持续更新动画，这是正常动画需求，但若弹幕关闭时也保持 timer 运行，会有低效风险。
- `DanmakuPanel::highlightByTime()` 每次位置变化线性扫描全部弹幕，弹幕量大时可能成为热点。
- 旧文件 `danmakuinput.h`、`danmakudisplay.h`、`danmakuwidget.h` 是大头文件实现，容易被误 include。

#### 修改建议

- 删除或移动旧弹幕文件到 `tools/legacy/danmaku/`，并更新 `docs/legacy_inventory.md`。
- `media_id` 改为可选多策略：优先读取文件内容 hash 或嵌入式指纹，路径只作为 fallback。
- `DanmakuPanel` 高亮使用当前索引附近二分查找，而不是每次从头线性扫描。
- `DanmakuOverlay::setEnabled(false)` 时可停止动画 timer，重新开启时再启动。
- 增加弹幕 repository 测试，覆盖新增、按 media_id 查询、legacy video_path 迁移、按用户统计。

#### 修改优先级

高优先级。弹幕模块已经重构到关键节点，尽快清理旧文件和补测试可以避免回退。

### AI 聊天模块

#### 相关文件

- `aichatwidget.h`
- `aichatwidget.cpp`
- `aichatcontroller.h`
- `aichatcontroller.cpp`
- `aichatview.h`
- `aichatview.cpp`
- `aichatpanel.h`
- `aichatpanel.cpp`
- `network/aichatservice.h`
- `network/aichatservice.cpp`
- `captureservice.h`
- `framecaptureservice.h`
- `tools/legacy/gen_aichat.py`

#### 当前职责

AI 模块负责展示侧边聊天面板、截取当前视频画面、读取 API 配置、构造多模态请求、取消请求、解析模型响应和展示对话气泡。

#### 主要问题

- AI 模块已拆为 view/controller/service/panel，比旧单体实现清晰。
- `AiChatView` 仍有 300 多行 UI 构建和大量内联样式，长期维护成本较高。
- `AiChatView::scrollToBottom()` 使用 `QTimer::singleShot(50)` 等布局完成后滚动，这是 UI 时序 workaround。问题不严重，但属于不稳定 UI 逻辑。
- 截图依赖共享的 `CaptureService/FrameCaptureService`，仍受屏幕截图方案限制。
- `AiChatService` 只在构造时读取环境变量，运行期间修改环境变量不会刷新；通常可接受，但 UI 没有“重新读取配置”的明确入口。
- `tools/legacy/gen_aichat.py` 虽已默认拒绝运行，但仍具备硬编码绝对路径写源码能力。

#### 修改建议

- 将 `AiChatView` 样式迁移到 QSS，保留 `role/component` 属性，减少内联样式。
- 用 `QScrollArea` 内容布局变化信号或 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 替代固定 50ms 延时。
- `AiChatService` 提供 `reloadConfiguration()`，AI 面板显示“重新检查配置”入口。
- 长期将截图能力从窗口截图升级为视频帧提供器，截图、录制和 AI 共用同一接口。
- 将 `gen_aichat.py` 移到仓库外归档，或让它只打印示例代码，不再写源文件。

#### 修改优先级

中优先级。AI 模块边界已经较好，后续重点是样式、截图可靠性和遗留脚本。

### 资源与样式模块

#### 相关文件

- `resources/resources.qrc`
- `resources/styles/app.qss`
- `assets/*.png`
- `uitheme.h`
- `uitheme.cpp`
- `audiostyle.h`
- `audiostyle.cpp`
- 多个 UI 类中的 `setStyleSheet()`

#### 当前职责

资源模块打包图标和全局样式。`UiTheme` 加载 `:/styles/app.qss`。`AudioStyle` 通过动态属性给音频 UI 设置 component/role，`VideoControlBar` 也开始使用 role 属性。

#### 主要问题

- 全局 QSS 已有 600 多行，但 `AiChatView`、`AuthDialogController`、`DanmakuInputBar`、`DanmakuPanel`、`UnifiedHistoryDialog`、`LyricWidget`、`VideoPlayerWidget` 仍有大量内联 `setStyleSheet()`。
- `resources.qrc` 引用 `../assets/...`，目录间耦合较强。
- 字体大量指定 `Microsoft YaHei`，在非 Windows 环境可能回退不一致。
- 颜色主题仍偏深色蓝紫系，虽符合当前风格，但模块内渐变和内联色过多，统一调色困难。
- 控制按钮部分使用文字缩写而不是图标，视觉一致性不如 qrc 图标。

#### 修改建议

- 按模块拆 QSS：`app.qss`、`audio.qss`、`video.qss`、`danmaku.qss`、`ai.qss`，再由 qrc 统一加载。
- 给所有模块统一使用 `component` 和 `role` 属性，减少内联样式。
- 将 `assets/` 移入 `resources/assets/`，让 qrc 路径更稳定，或至少在文档中说明目录关系。
- 为跨平台字体定义字体栈，不在每个控件上重复写 `Microsoft YaHei`。
- 视频控制栏改用 qrc 图标和 tooltip。

#### 修改优先级

低到中优先级。样式不阻塞核心功能，但会持续增加 UI 维护成本。

### 测试模块

#### 相关文件

- `tests/audio_manual_verification.md`
- `QtMediaPlayer.pro`

#### 当前职责

当前测试资料是音频模块手工验证清单，覆盖播放列表、控制器、歌词和回归流程。项目没有自动化测试 target。

#### 主要问题

- 无 QTest 自动化测试。
- 无数据库迁移测试，`MigrationRunner` 风险较高。
- 无 repository 测试，历史、用户、弹幕 SQL 无自动验证。
- 无网络解析测试，Bilibili/网易云/AI 响应格式变化无法提前发现。
- 无 UI smoke test，主窗口、音视频页面和关键对话框无法自动启动验证。
- `.gitignore` 目前忽略 `test_*.cpp` 和 `*_test.cpp`，如果后续按常见命名添加测试文件，需要调整规则或使用 `tests/auto/*.cpp` 并显式放行。

#### 修改建议

- 新增 `tests/auto/`，使用 QTest 覆盖 `PlaylistModel`、`LyricParser`、`MediaHistoryRecord`、`NetworkClient`。
- 对 `MigrationRunner` 使用测试数据库或容器化 MySQL，至少覆盖空库迁移和旧版本升级。
- Repository 层通过 `IDatabaseProvider` 注入测试 provider。
- 对 Bilibili/网易云/AI 解析准备固定 JSON fixture，不依赖真实网络。
- 在 `.pro` 或后续 CMake 中增加测试 target，纳入 CI。

#### 修改优先级

高优先级。当前项目处于重构中后期，没有测试会让清理遗留代码和数据库迁移风险很高。

### 构建配置模块

#### 相关文件

- `QtMediaPlayer.pro`
- `编译项目.bat`
- `.gitignore`
- `database.ini.example`

#### 当前职责

`.pro` 配置 Qt 模块、C++17、include path、构建输出目录、源码列表、资源和 Windows `dwmapi` 链接。`编译项目.bat` 在 Windows 上创建 `build/qmake` 并运行 qmake/mingw32-make。`.gitignore` 排除编译产物和本地配置。

#### 主要问题

- `.pro` 源码列表过长，虽然按一定顺序排列，但没有按模块加注释分段。
- `编译项目.bat` 直接设置 `QT_DIR=D:\QT\6.5.3\mingw_64` 和 `MINGW_DIR=D:\QT\Tools\mingw1120_64`，会覆盖用户环境变量，换机器容易失败。
- 部署依赖没有脚本化检查：QMYSQL plugin、`libmysql.dll`、MySQL 服务、`database.ini`、FFmpeg、Qt multimedia backend、AI 环境变量。
- 当前 `.gitignore` 未包含 `docs/generated/`。
- `.gitignore` 忽略 `test_*.cpp` 和 `*_test.cpp`，可能和未来测试命名冲突。
- 项目仍使用 qmake，Qt 6 长期工程更适合 CMake 管理模块 target 和测试 target。

#### 修改建议

- 短期在 `.pro` 中按 app/audio/video/history/auth/network/danmaku/ai/resources 分段注释。
- 修改批处理脚本：如果外部已设置 `QT_DIR`/`MINGW_DIR`，不要覆盖；未设置时再使用默认路径。
- 新增 `docs/deployment_windows.md`，列出运行必需文件和环境变量。
- `.gitignore` 增加 `docs/generated/`，并为未来测试文件放行 `tests/auto/**/*.cpp`。
- 中期迁移 CMake，建立 `QtMediaPlayerApp`、`core`、`network`、`tests` 等 target。

#### 修改优先级

中优先级。当前本机可构建，但跨机器复现和发布部署风险明显。

### 遗留代码与生成物模块

#### 相关文件

- `danmakumanager.h`
- `danmakumanager.cpp`
- `danmakuinput.h`
- `danmakudisplay.h`
- `danmakuwidget.h`
- `lyricdownloader.h`
- `tools/legacy/gen_aichat.py`
- `docs/legacy_inventory.md`
- `docs/generated/ai-model-hub.html`

#### 当前职责

这些文件主要是兼容层、历史实现或生成物。旧弹幕文件已记录在 `docs/legacy_inventory.md`，且不再列入 `.pro`。`LyricDownloader` 是 `LyricDownloadService` 的 deprecated 包装。`gen_aichat.py` 是历史源码追加脚本。

#### 主要问题

- 旧弹幕文件虽然不编译，但仍在根目录，搜索结果和 IDE 补全会混淆新旧实现。
- `LyricDownloader` 仍在 `.pro` 的 HEADERS 中，且头文件内有完整实现。
- `gen_aichat.py` 仍硬编码绝对路径并能修改源码，只是默认要求显式参数。
- `docs/generated/ai-model-hub.html` 与项目主文档混在一起，像生成产物。

#### 修改建议

- 将旧弹幕文件移动到 `tools/legacy/danmaku/` 或在确认无引用后删除。
- 从 `.pro` 移除 `lyricdownloader.h`，所有调用迁移到 `LyricService`。
- 将 `gen_aichat.py` 改为只读示例，或移出仓库。
- 将 `docs/generated/` 加入 `.gitignore`，需要保留时另写说明。

#### 修改优先级

中优先级。遗留文件不一定影响运行，但会持续干扰重构和审查。

## 4. 重点问题检查结论

1. 是否存在重复模块或重复类？  
   存在，但比旧状态少。历史模块已基本收敛到 `MediaHistoryService + UnifiedHistoryDialog + MediaPlaybackRouter`；当前主要重复在旧弹幕文件与新 `danmaku/` 模块并存、`LyricDownloader` 与 `LyricService/LyricDownloadService` 并存、`UserAccountService/DBManager` 兼容命名与 `AuthService/UserRepository` 边界重叠。

2. 是否存在已经废弃但仍然保留的代码？  
   存在。`danmakumanager.*`、`danmakuinput.h`、`danmakudisplay.h`、`danmakuwidget.h` 是 legacy；`lyricdownloader.h` 标记 deprecated；`DBManager` alias 和若干 `UserAccountService` 方法标记 deprecated；`tools/legacy/gen_aichat.py` 是历史脚本。

3. 是否有头文件中写了大量实现？  
   有。最明显的是 `spectrumwidget.h`、`danmakuinput.h`、`danmakudisplay.h`、`danmakuwidget.h`、`lyricdownloader.h`。`audiotrack.h` 中的 `displayText()` 属于小型 inline，可接受。

4. 是否有 UI 代码和业务逻辑耦合严重的问题？  
   有。`Widget` 创建服务和页面；`AudioPlayer` 同时管 UI、服务和交互；`VideoPlayerWidget` 同时建 UI 和服务；`AuthDialogController` 构建修改密码 UI；`VideoHistoryCoordinator` 直接弹恢复播放对话框；`OnlineMusicService` 创建搜索对话框。

5. 是否有数据库访问逻辑散落在 UI 层？  
   直接 SQL 主要集中在 `UserRepository`、`MediaHistoryRepository`、`DanmakuRepository`、`DatabaseManager`、`MigrationRunner`，UI 层没有大量 `QSqlQuery`。但 UI 层仍创建会触发数据库的服务对象，数据库可用性没有集中传递为功能开关。

6. 是否有网络失败后伪造成功结果的问题？  
   未发现普遍伪造成功。`NetworkClient` 和各服务大多显式失败；在线视频 DASH/不可直播会返回 BrowserOnly 并警告；在线音频加入播放列表时文案明确“播放前会解析真实播放地址”。需要微调的是 `OnlineVideoSearch` 的“已发送到播放器”文案，它不是解析成功。

7. 是否有资源路径依赖工作目录的问题？  
   运行时图标和 QSS 已走 qrc，较好。风险主要是 `编译项目.bat` 硬编码 Qt/Mingw 路径，`VideoEncoder` 保留 Windows 绝对 FFmpeg 候选，`tools/legacy/gen_aichat.py` 硬编码源码路径，数据库配置优先读取应用目录 `database.ini`。

8. 是否有播放历史、视频历史、音频历史重复实现？  
   当前源码中没有独立 `playhistory.*` 或 `videohistory.*`，历史已基本统一。仍存在 `VideoHistoryCoordinator` 作为视频 UI 协调层，以及数据库里旧 `video_history` 迁移逻辑。真正未解决的是 `play_history.file_path` 唯一键和在线媒体结构化身份。

9. 是否有 QTimer 延时恢复状态这种不稳定逻辑？  
   视频恢复进度没有使用固定延时，而是 pending seek 配合 `mediaStatusChanged`，方向正确。仍有 `Widget` 的 0ms 启动警告、`AiChatView` 50ms 滚动到底、`NetworkClient` 重试延时、`VideoPlayerController` 5秒保存进度、`DanmakuOverlay` 30ms 动画、`VideoCapture` 200ms 截帧、`SpectrumWidget` 50ms 模拟动画。其中最像 workaround 的是 AI 滚动 50ms。

10. 是否有 qDebug 过多的问题？  
   只有中低程度问题。`NetworkClient` 已使用 `QLoggingCategory` 且默认静默；主要剩余是 `LyricParser` 多处 `qDebug()` 和 `AuthService` 初始化失败 `qDebug()`。建议统一日志分类。

11. 是否有应该进入 `.gitignore` 的文件？  
   `.gitignore` 已覆盖 `database.ini`、`*.dll`、`*.lib`、`bin/`、`obj/`、`moc/`、`rcc/`、`ui/`、`build/`、`build-*`、`Makefile*`。建议补充 `docs/generated/`，并审视 `tools/legacy/query` 是否属于本机缓存。

12. 是否有测试缺失？  
   明显缺失。当前只有手工音频验证文档，没有 QTest、没有 repository 测试、没有迁移测试、没有网络解析样本测试、没有 UI smoke test。

13. 是否有命名不清晰的问题？  
   有。`Widget` 太泛；`VideoPlayerWidget` 不是 QWidget；`VideoPlayer` 是 alias；`controlPannel` 拼写错误；`m_btnCtr` 含义不清；`DBManager` 和 `DatabaseManager` 易混；`OnlineMusicService::search/searchMusic/searchSongsAsync` 边界重复。

14. 是否有可能导致部署失败的问题？  
   有。QMYSQL plugin、MySQL 服务、`libmysql.dll`、`database.ini`、Qt multimedia backend、FFmpeg、硬编码 Qt/Mingw 路径、AI 环境变量、第三方网络接口变化、字体差异都会导致跨机器运行失败。当前缺少发布检查清单和自动化部署脚本。

## 重构路线图

### 第一阶段：低风险修复

- 将 `LyricParser` 和 `AuthService` 的裸 `qDebug()` 改为 `QLoggingCategory`。
- 修改 `OnlineVideoSearch` 文案：播放选中后显示“正在解析播放地址”，成功由 resolver 再提示。
- 在 `.gitignore` 中加入 `docs/generated/`，明确 `tools/legacy/query` 是否保留。
- 修改 `编译项目.bat`，支持外部 `QT_DIR`、`MINGW_DIR` 环境变量覆盖。
- 新增 `docs/deployment_windows.md`，记录 QMYSQL、libmysql、database.ini、FFmpeg、Qt plugins、AI 环境变量。
- 从普通 UI 隐藏或条件编译 `AudioPlayer::testAudio()`。
- 修正 `VideoControlBar` 的 `controlPannel` 拼写和 `m_btnCtr` 命名。

### 第二阶段：模块合并与职责拆分

- 将旧弹幕文件移入 `tools/legacy/danmaku/` 或删除，并更新 `docs/legacy_inventory.md`。
- 从 `.pro` 移除 `lyricdownloader.h`，统一使用 `LyricService`。
- 将 `VideoPlayerWidget` 重命名为 `VideoPlayerFacade`，同步更新相关 alias 和调用方。
- 拆分 `AudioPlayerController`：播放列表控制、在线音频解析、历史记录分别成类。
- 拆分 `BilibiliPlaybackResolver` 中的 URL 构造和 JSON 解析逻辑。
- 将 `VideoHistoryCoordinator` 的恢复提示弹窗移到 UI 层。
- 将 `SpectrumWidget` 移到 `.cpp` 并重命名为 `SimulatedSpectrumWidget`，或接入真实音频采样。

### 第三阶段：架构优化

- 引入 `AppServices/ApplicationContext`，集中创建 `UserSession`、`AuthService`、`MediaHistoryService`、网络服务和数据库状态。
- 迁移 `play_history` schema，增加 `source_type/source_id/source_url/title/artist/thumbnail`，唯一键从 `file_path` 调整为结构化身份。
- 设计稳定本地媒体 ID，历史和弹幕不再主要依赖绝对路径。
- 将 UI 弹窗从 service/coordinator 层抽离，统一通过结果对象或信号表达业务状态。
- 统一样式体系，把 AI、认证、弹幕、历史对话框内联样式迁移到 QSS。
- 评估从 qmake 迁移到 CMake，建立模块 target 和测试 target。

### 第四阶段：测试与质量保障

- 使用 QTest 覆盖 `PlaylistModel`、`LyricParser`、`MediaHistoryRecord`、`AudioPlaybackController`。
- 为 `NetworkClient` 增加超时、取消、重试、HTTP 错误和日志脱敏测试。
- 为 `MigrationRunner` 增加空库迁移、旧库迁移、失败回滚测试。
- 为 `UserRepository`、`MediaHistoryRepository`、`DanmakuRepository` 增加测试数据库覆盖。
- 为 Bilibili、网易云音乐、AI 响应解析准备固定 JSON fixture。
- 增加最小 CI：qmake/CMake 构建、QTest、资源存在性检查、部署清单检查。

## 可执行任务清单

- [ ] 在 `.gitignore` 中加入 `docs/generated/`，并确认 `tools/legacy/query` 的保留策略。
- [ ] 新增 `docs/deployment_windows.md`，记录 QMYSQL、libmysql.dll、database.ini、FFmpeg、Qt plugins 和 AI 环境变量。
- [ ] 修改 `编译项目.bat`，只在外部未设置时使用默认 `QT_DIR` 和 `MINGW_DIR`。
- [ ] 将 `AuthService` 初始化失败日志从 `qDebug()` 改为 `QLoggingCategory`。
- [ ] 将 `LyricParser` 的调试输出改为 `QLoggingCategory` 并默认关闭。
- [ ] 修改 `OnlineVideoSearch::onPlaySelected()` 状态文案为“正在解析播放地址”。
- [ ] 修正 `VideoControlBar` 的 `controlPannel` 对象名拼写。
- [ ] 将 `VideoControlBar::m_btnCtr` 重命名为更清晰的 `m_compactPlayButton`。
- [ ] 将 `AudioPlayer::testAudio()` 移到 debug-only 或开发者菜单。
- [ ] 从 `.pro` 移除 `lyricdownloader.h`，确认无生产代码依赖 `LyricDownloader`。
- [ ] 将 `danmakumanager.*`、`danmakuinput.h`、`danmakudisplay.h`、`danmakuwidget.h` 移入 legacy 目录或删除。
- [ ] 更新 `docs/legacy_inventory.md`，补充 `LyricDownloader` 和 `gen_aichat.py` 的删除条件。
- [ ] 将 `VideoPlayerWidget` 重命名为 `VideoPlayerFacade`。
- [ ] 将 `Widget` 中服务创建迁移到新的 `AppServices` 或 `ApplicationContext`。
- [ ] 抽出统一 `MediaTypeDetector`，供打开文件对话框和播放模块共享格式列表。
- [ ] 将 `MediaPlaybackRouter` 的视频历史播放增加本地文件存在性检查。
- [ ] 删除 `UnifiedHistoryDialog` 的旧 `playAudio/playVideo` 信号路径，只保留 `playRequested(record)`。
- [ ] 新增数据库迁移，为 `play_history` 增加 `source_type`、`source_id`、`source_url`、`title`、`artist`。
- [ ] 将 `play_history` 唯一键从单 `file_path` 调整为结构化媒体身份。
- [ ] 将在线音频历史从 `online-audio:` query 过渡到结构化列写入。
- [ ] 将 `VideoHistoryCoordinator::restoreProgressIfNeeded()` 的 `QMessageBox` 移到 UI 层。
- [ ] 拆分 `AudioPlayerController` 中的在线解析和历史记录逻辑。
- [ ] 将 `OnlineMusicService` 的 `searchFinished(QStringList)` 标记 deprecated 或删除。
- [ ] 为 `OnlineMusicService` 搜索和 URL 解析增加 JSON fixture 测试。
- [ ] 拆分 `BilibiliPlaybackResolver` 的 JSON 解析为独立 parser。
- [ ] 为 Bilibili 搜索、cid、durl、DASH-only、接口错误增加解析测试。
- [ ] 将 `SpectrumWidget` 移到 `.cpp` 并重命名为 `SimulatedSpectrumWidget`。
- [ ] 评估 `FrameCaptureService` 使用 `QVideoSink` 或其他真实帧捕获方案。
- [ ] 将 `AiChatView` 大段内联样式迁移到 QSS。
- [ ] 将 `AiChatView::scrollToBottom()` 的 50ms 定时改为 queued layout 更新。
- [ ] 为 `AiChatService::extractReplyText()` 增加 OpenAI/DashScope 响应样本测试。
- [ ] 为 `PlaylistModel` 增加 QTest。
- [ ] 为 `LyricParser::parseLrcText()` 增加 QTest。
- [ ] 为 `NetworkClient` 增加超时、取消、重试、HTTP 4xx/5xx 测试。
- [ ] 为 `MigrationRunner` 增加空库迁移和旧库升级测试。
- [ ] 为 `DanmakuRepository` 增加 media_id 查询和 legacy 行迁移测试。
- [ ] 规划 CMake 迁移，拆分 app/audio/video/history/auth/network/danmaku/ai/tests target。
