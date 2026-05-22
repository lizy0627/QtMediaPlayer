# QtMediaPlayer

QtMediaPlayer 是一个基于 **Qt 6 / C++17** 开发的智能影音播放器项目，面向桌面端本地媒体播放、在线音视频检索、播放历史管理、歌词与弹幕互动等场景。项目采用较清晰的 Controller / Service / Repository 分层设计，适合作为 Qt/C++ 桌面应用、音视频处理、网络请求与数据库持久化能力的综合展示项目。

## 核心功能

- 本地音频播放：支持常见音频文件导入、播放、暂停、上一首/下一首、音量控制和播放列表管理。
- 本地视频播放：支持本地视频打开、播放控制、进度跳转、倍速播放和播放队列。
- 播放历史：记录音频和视频播放历史，支持恢复播放进度。
- 歌词显示/下载：支持本地歌词解析、在线歌词下载和播放进度同步显示。
- 弹幕系统：支持弹幕发送、展示、列表管理、历史弹幕查询和用户弹幕记录。
- 用户登录：提供基础用户登录、会话状态管理和用户相关数据隔离。
- 在线音乐/在线视频搜索：集成第三方接口进行在线音乐和 Bilibili 视频搜索，并在播放前解析真实播放地址。
- 截图/录制：支持视频画面截图，以及无声画面录制与转码流程。
- SQLite/MySQL 数据存储：默认使用 SQLite，本地开发开箱即用；也支持切换到 MySQL。

## 技术栈

- 语言与标准：C++17
- GUI 框架：Qt 6 Widgets
- 音视频能力：Qt Multimedia、Qt Multimedia Widgets
- 网络请求：Qt Network、异步请求封装、超时与轻量重试
- 数据存储：Qt SQL、SQLite、MySQL
- 数据解析：JSON、LRC 歌词解析
- 构建工具：qmake、MinGW
- 平台：Windows 桌面端

## 模块架构

项目按职责拆分为多个协作模块，避免将所有逻辑集中在窗口类中。

- UI 层：负责页面布局、控件展示和用户交互，例如 `widget.*`、`audiocontrolbar.*`、`videocontrolbar.*`、`playlistpanel.*`、`lyricwidget.*`。
- Controller 层：协调 UI、播放控制、业务流程和状态变化，例如 `audioplayercontroller.*`、`videoplayercontroller.*`、`mainwindowcontroller.*`、`aichatcontroller.*`。
- Service 层：封装业务能力，例如播放历史、歌词加载、截图录制、在线搜索、认证和媒体探测。
- Repository/Database 层：负责数据持久化与数据库访问，例如 `dbmanager.*`、`databasecontext.*`、`userrepository.*`、`danmaku/danmakurepository.*`、`mediahistory.*`。
- Network 层：封装第三方接口和网络请求，例如 `network/networkclient.*`、`network/onlinemusicservice.*`、`network/onlinevideoservice.*`、`network/bilibilisearchservice.*`、`network/bilibiliplaybackresolver.*`。

## 运行环境

- Qt 6.x，建议 Qt 6.5 或更高版本
- C++17 编译器
- MinGW 64-bit 工具链
- Windows 10/11
- 可选：MySQL Server
- 可选：FFmpeg，用于录制后的视频转码流程

项目使用到的 Qt 模块包括：

- `core`
- `gui`
- `widgets`
- `multimedia`
- `multimediawidgets`
- `network`
- `sql`
- `concurrent`

## 构建步骤

### 方式一：使用脚本构建

项目提供了 Windows 构建脚本：

```powershell
./编译项目.bat
```

脚本默认使用以下路径，请根据本机 Qt 安装位置调整脚本中的变量：

```bat
set "QT_DIR=D:\QT\6.5.3\mingw_64"
set "MINGW_DIR=D:\QT\Tools\mingw1120_64"
```

构建成功后，输出目录为：

```text
build/qmake/bin
```

### 方式二：手动 qmake 构建

```powershell
mkdir build/qmake
cd build/qmake
qmake ../../QtMediaPlayer.pro
mingw32-make -j4
```

如果 `qmake` 或 `mingw32-make` 无法识别，请先把 Qt 和 MinGW 的 `bin` 目录加入 `PATH`。

## 数据库配置

QtMediaPlayer 通过 Qt SQL 同时支持 `QSQLITE` 和 `QMYSQL`。默认情况下可以直接使用 SQLite 本地数据库运行，无需手动安装或启动 MySQL。

### 默认 SQLite

如果没有提供 `database.ini`，也没有设置数据库相关环境变量，程序会自动使用 SQLite。默认数据库文件保存在 `QStandardPaths::AppDataLocation` 对应的应用数据目录下，文件名为：

```text
qtmediaplayer.sqlite3
```

这是本地开发和普通桌面使用的推荐方式。也可以通过 `database.ini` 指定 SQLite 数据库文件位置：

```ini
[Database]
driver=QSQLITE
name=C:/data/qtmediaplayer.sqlite3
```

也可以使用环境变量指定 SQLite：

```powershell
$env:QTMEDIAPLAYER_DB_DRIVER="QSQLITE"
$env:QTMEDIAPLAYER_DB_NAME="C:/data/qtmediaplayer.sqlite3"
```

### 可选 MySQL

如果需要使用 MySQL，可以在 `database.ini` 或环境变量中配置 `QMYSQL`。使用 MySQL 时，需要本机或远程 MySQL 服务可用，并确保 Qt 的 QMYSQL driver 可以正常加载。

MySQL 版 `database.ini` 示例。请将示例中的用户名和密码替换为自己的 MySQL 账号，不要把示例值当作生产环境推荐配置：

```ini
[Database]
driver=QMYSQL
host=127.0.0.1
port=3306
name=qtmediaplayer
user=qtmediaplayer_user
password=change-this-password
createDatabase=true
connectOptions=MYSQL_OPT_RECONNECT=1
```

也可以使用环境变量配置 MySQL：

```powershell
$env:QTMEDIAPLAYER_DB_DRIVER="QMYSQL"
$env:QTMEDIAPLAYER_DB_HOST="127.0.0.1"
$env:QTMEDIAPLAYER_DB_PORT="3306"
$env:QTMEDIAPLAYER_DB_NAME="qtmediaplayer"
$env:QTMEDIAPLAYER_DB_USER="qtmediaplayer_user"
$env:QTMEDIAPLAYER_DB_PASSWORD="change-this-password"
```

如果 MySQL 初始化失败，例如连接失败、数据库驱动不可用或建库失败，程序会尝试 fallback 到本地 SQLite 数据库，以便登录数据、播放历史和弹幕等功能仍可继续使用。

开发用种子用户默认关闭。只有在一次性本地数据库中调试时，才建议设置：

```powershell
$env:QTMEDIAPLAYER_SEED_DEV_USERS="1"
```

## FAQ

### 为什么某些 mp4 播放不了？

`mp4` 只是封装格式，内部视频编码可能是 H.264、H.265/HEVC、AV1 等。Qt Multimedia 最终依赖系统媒体后端和本机解码器能力，扩展名为 `.mp4` 不代表一定可播放。可以尝试换用更常见的 H.264 + AAC 编码，或安装对应系统解码器。

### 为什么在线音乐/视频不可用？

在线音乐和在线视频依赖第三方接口，播放地址可能为空、过期、不可访问，或被版权、登录、Referer、防盗链策略限制。项目会在播放前解析真实地址，并在失败时给出提示；如果失败，可以重新搜索或重新播放触发重新解析。

### QMYSQL 驱动缺失怎么办？

如果提示 `QMYSQL driver not loaded`，说明当前 Qt 运行环境没有可用的 MySQL SQL driver，或缺少 MySQL 客户端动态库。可以选择：

- 直接使用默认 SQLite，不配置 MySQL。
- 检查 Qt 的 `sqldrivers` 目录中是否存在 `qsqlmysql.dll`。
- 确保 MySQL 客户端库，例如 `libmysql.dll`，位于程序目录或系统 `PATH` 中。
- 使用与当前 Qt/MinGW ABI 匹配的 QMYSQL 驱动。

### 默认 SQLite 数据库在哪里？

默认 SQLite 数据库位于 Qt 的 `QStandardPaths::AppDataLocation` 应用数据目录下，文件名是：

```text
qtmediaplayer.sqlite3
```

如果想固定数据库位置，可以通过 `database.ini` 或 `QTMEDIAPLAYER_DB_NAME` 环境变量显式指定。
