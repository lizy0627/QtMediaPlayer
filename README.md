# QtMediaPlayer

QtMediaPlayer is a Qt Widgets media player with local playback history, user login data, and danmaku storage.

## Requirements

- Qt 6.x
- C++17
- Qt Multimedia
- Qt Network
- Qt SQL

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

Development seed users are disabled by default. Set `QTMEDIAPLAYER_SEED_DEV_USERS=1` only for disposable local databases.
