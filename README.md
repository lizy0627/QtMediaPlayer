# QtMediaPlayer

QtMediaPlayer is a Qt Widgets media player with local playback history, user login data, and danmaku storage.

## Database

The application supports both SQLite and MySQL through Qt SQL.

### SQLite default

If no `database.ini` and no database environment variables are configured, QtMediaPlayer uses SQLite automatically. The database file is created under `QStandardPaths::AppDataLocation` as:

```text
qtmediaplayer.sqlite3
```

This is the recommended mode for local development and normal desktop use. To choose a custom SQLite file, create `database.ini`:

```ini
[Database]
driver=QSQLITE
name=C:/data/qtmediaplayer.sqlite3
```

Or use environment variables:

```powershell
$env:QTMEDIAPLAYER_DB_DRIVER="QSQLITE"
$env:QTMEDIAPLAYER_DB_NAME="C:/data/qtmediaplayer.sqlite3"
```

### MySQL optional

MySQL remains supported. If `database.ini` or environment variables configure `QMYSQL`, the application tries MySQL first. If that connection or driver is unavailable, it falls back to the local SQLite database so login, playback history, and danmaku features can still work.

Example `database.ini`:

```ini
[Database]
driver=QMYSQL
host=127.0.0.1
port=3306
name=qtmediaplayer
user=root
password=123456
createDatabase=true
connectOptions=MYSQL_OPT_RECONNECT=1
```

Environment variable form:

```powershell
$env:QTMEDIAPLAYER_DB_DRIVER="QMYSQL"
$env:QTMEDIAPLAYER_DB_HOST="127.0.0.1"
$env:QTMEDIAPLAYER_DB_PORT="3306"
$env:QTMEDIAPLAYER_DB_NAME="qtmediaplayer"
$env:QTMEDIAPLAYER_DB_USER="root"
$env:QTMEDIAPLAYER_DB_PASSWORD="123456"
```

Development seed users are disabled by default. Set `QTMEDIAPLAYER_SEED_DEV_USERS=1` only for disposable local databases.
