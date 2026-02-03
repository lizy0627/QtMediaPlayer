#ifndef ONLINEMUSICSEARCH_H
#define ONLINEMUSICSEARCH_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QMessageBox>
#include <QProgressBar>
#include <QDebug>
#include <QUrl>
#include <QUrlQuery>

// 歌曲信息结构
struct SongInfo
{
    QString id;           // 歌曲ID
    QString name;         // 歌曲名称
    QString artist;       // 艺术家
    QString album;        // 专辑
    QString url;          // 播放URL
    QString lyricUrl;     // 歌词URL
    int duration;         // 时长（秒）
    
    SongInfo() : duration(0) {}
};

// 在线音乐搜索对话框
class OnlineMusicSearch : public QDialog
{
    Q_OBJECT
    
private:
    QLineEdit* m_searchEdit;           // 搜索输入框
    QPushButton* m_searchButton;       // 搜索按钮
    QListWidget* m_resultList;         // 搜索结果列表
    QProgressBar* m_progressBar;       // 进度条
    QLabel* m_statusLabel;             // 状态标签
    
    QNetworkAccessManager* m_networkManager;  // 网络管理器
    QList<SongInfo> m_songs;                  // 歌曲列表
    
public:
    explicit OnlineMusicSearch(QWidget* parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("在线音乐搜索");
        setMinimumSize(800, 600);
        setupUI();
        
        m_networkManager = new QNetworkAccessManager(this);
        connect(m_networkManager, &QNetworkAccessManager::finished,
                this, &OnlineMusicSearch::onSearchFinished);
    }
    
    // 获取选中的歌曲信息
    SongInfo getSelectedSong() const
    {
        int row = m_resultList->currentRow();
        if (row >= 0 && row < m_songs.size()) {
            return m_songs[row];
        }
        return SongInfo();
    }
    
signals:
    void songSelected(const SongInfo& song);
    
private:
    void setupUI()
    {
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(15);
        mainLayout->setContentsMargins(20, 20, 20, 20);
        
        // 设置对话框样式
        setStyleSheet(
            "QDialog { "
            "   background-color: #2b2b2b; "
            "}"
            "QLineEdit { "
            "   background-color: #1e1e1e; "
            "   color: #ffffff; "
            "   border: 2px solid #444; "
            "   border-radius: 8px; "
            "   padding: 10px; "
            "   font-size: 14pt; "
            "}"
            "QLineEdit:focus { "
            "   border: 2px solid #64b5f6; "
            "}"
            "QPushButton { "
            "   background-color: #0d47a1; "
            "   color: white; "
            "   border: none; "
            "   padding: 10px 20px; "
            "   border-radius: 8px; "
            "   font-weight: bold; "
            "   font-size: 12pt; "
            "}"
            "QPushButton:hover { "
            "   background-color: #1565c0; "
            "}"
            "QPushButton:pressed { "
            "   background-color: #0a3d91; "
            "}"
            "QPushButton:disabled { "
            "   background-color: #555; "
            "   color: #888; "
            "}"
            "QListWidget { "
            "   background-color: #1e1e1e; "
            "   color: #ffffff; "
            "   border: 2px solid #444; "
            "   border-radius: 8px; "
            "   padding: 5px; "
            "   font-size: 11pt; "
            "}"
            "QListWidget::item { "
            "   padding: 12px; "
            "   border-bottom: 1px solid #333; "
            "   border-radius: 4px; "
            "}"
            "QListWidget::item:hover { "
            "   background-color: #3a3a3a; "
            "}"
            "QListWidget::item:selected { "
            "   background-color: #0d47a1; "
            "   color: white; "
            "}"
            "QLabel { "
            "   color: #ffffff; "
            "   font-size: 11pt; "
            "}"
            "QProgressBar { "
            "   border: 2px solid #444; "
            "   border-radius: 5px; "
            "   text-align: center; "
            "   background-color: #1e1e1e; "
            "   color: white; "
            "}"
            "QProgressBar::chunk { "
            "   background-color: #64b5f6; "
            "   border-radius: 3px; "
            "}"
        );
        
        // 标题
        QLabel* titleLabel = new QLabel("🎵 在线音乐搜索", this);
        titleLabel->setStyleSheet(
            "font-size: 18pt; "
            "font-weight: bold; "
            "color: #64b5f6; "
            "padding: 10px;"
        );
        titleLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(titleLabel);
        
        // 搜索栏
        QHBoxLayout* searchLayout = new QHBoxLayout();
        searchLayout->setSpacing(10);
        
        m_searchEdit = new QLineEdit(this);
        m_searchEdit->setPlaceholderText("请输入歌曲名称或艺术家...");
        searchLayout->addWidget(m_searchEdit, 1);
        
        m_searchButton = new QPushButton("🔍 搜索", this);
        m_searchButton->setFixedWidth(120);
        searchLayout->addWidget(m_searchButton);
        
        mainLayout->addLayout(searchLayout);
        
        // 进度条
        m_progressBar = new QProgressBar(this);
        m_progressBar->setRange(0, 0);  // 不确定进度
        m_progressBar->hide();
        mainLayout->addWidget(m_progressBar);
        
        // 状态标签
        m_statusLabel = new QLabel("请输入关键词开始搜索", this);
        m_statusLabel->setStyleSheet("color: #64b5f6; font-style: italic;");
        m_statusLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(m_statusLabel);
        
        // 结果列表
        QLabel* resultLabel = new QLabel("搜索结果（双击播放）：", this);
        resultLabel->setStyleSheet("font-weight: bold; color: #64b5f6;");
        mainLayout->addWidget(resultLabel);
        
        m_resultList = new QListWidget(this);
        mainLayout->addWidget(m_resultList);
        
        // 底部按钮
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        buttonLayout->addStretch();
        
        QPushButton* playButton = new QPushButton("▶️ 播放选中", this);
        QPushButton* closeButton = new QPushButton("关闭", this);
        
        buttonLayout->addWidget(playButton);
        buttonLayout->addWidget(closeButton);
        
        mainLayout->addLayout(buttonLayout);
        
        // 连接信号
        connect(m_searchEdit, &QLineEdit::returnPressed, this, &OnlineMusicSearch::onSearch);
        connect(m_searchButton, &QPushButton::clicked, this, &OnlineMusicSearch::onSearch);
        connect(m_resultList, &QListWidget::itemDoubleClicked, this, &OnlineMusicSearch::onPlaySelected);
        connect(playButton, &QPushButton::clicked, this, &OnlineMusicSearch::onPlaySelected);
        connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
    }
    
private slots:
    void onSearch()
    {
        QString keyword = m_searchEdit->text().trimmed();
        if (keyword.isEmpty()) {
            QMessageBox::warning(this, "提示", "请输入搜索关键词！");
            return;
        }
        
        m_statusLabel->setText("正在搜索：" + keyword);
        m_progressBar->show();
        m_searchButton->setEnabled(false);
        m_resultList->clear();
        m_songs.clear();
        
        // 使用网易云音乐API搜索（这里使用第三方API接口）
        // 注意：实际使用时需要替换为可用的API
        searchMusic(keyword);
    }
    
    void searchMusic(const QString& keyword)
    {
        // 使用免费的音乐API进行搜索
        // 这里使用一个示例API，实际项目中需要使用正规的音乐服务API
        
        // 方案1: 使用网易云音乐API（需要自建或使用第三方服务）
        QString apiUrl = QString("http://music.163.com/api/search/get/web?s=%1&type=1&offset=0&limit=30")
                            .arg(QUrl::toPercentEncoding(keyword).constData());
        
        // 方案2: 使用其他免费API（示例）
        // QString apiUrl = QString("https://api.example.com/search?keyword=%1")
        //                     .arg(QUrl::toPercentEncoding(keyword).constData());
        
        QNetworkRequest request;
        request.setUrl(QUrl(apiUrl));
        request.setHeader(QNetworkRequest::UserAgentHeader, 
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        request.setRawHeader("Referer", "http://music.163.com");
        
        m_networkManager->get(request);
    }
    
    void onSearchFinished(QNetworkReply* reply)
    {
        m_progressBar->hide();
        m_searchButton->setEnabled(true);
        
        if (reply->error() != QNetworkReply::NoError) {
            m_statusLabel->setText("搜索失败：" + reply->errorString());
            
            // 显示模拟数据用于演示
            showDemoResults();
            
            reply->deleteLater();
            return;
        }
        
        QByteArray data = reply->readAll();
        reply->deleteLater();
        
        parseSearchResults(data);
    }
    
    void parseSearchResults(const QByteArray& data)
    {
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (doc.isNull() || !doc.isObject()) {
            m_statusLabel->setText("解析结果失败");
            showDemoResults();
            return;
        }
        
        QJsonObject root = doc.object();
        QJsonObject result = root["result"].toObject();
        QJsonArray songs = result["songs"].toArray();
        
        if (songs.isEmpty()) {
            m_statusLabel->setText("未找到相关歌曲");
            showDemoResults();
            return;
        }
        
        m_songs.clear();
        m_resultList->clear();
        int validCount = 0;
        
        for (const QJsonValue& value : songs) {
            QJsonObject songObj = value.toObject();
            
            SongInfo song;
            song.id = QString::number(songObj["id"].toInt());
            song.name = songObj["name"].toString();
            song.duration = songObj["duration"].toInt() / 1000;
            
            // 获取艺术家
            QJsonArray artists = songObj["artists"].toArray();
            QStringList artistNames;
            for (const QJsonValue& artist : artists) {
                artistNames << artist.toObject()["name"].toString();
            }
            song.artist = artistNames.join(", ");
            
            // 获取专辑
            QJsonObject album = songObj["album"].toObject();
            song.album = album["name"].toString();
            
            // 构建播放URL
            song.url = QString("http://music.163.com/song/media/outer/url?id=%1.mp3").arg(song.id);
            
            // 检查歌曲是否可播放（简单验证：检查是否有VIP标记或其他限制）
            // 注意：这里只是基本过滤，实际可用性需要尝试播放才能确定
            bool isPlayable = true;
            
            // 检查是否有fee字段（0=免费, 1=VIP, 4=购买专辑, 8=低音质免费）
            if (songObj.contains("fee")) {
                int fee = songObj["fee"].toInt();
                if (fee == 1 || fee == 4) {
                    isPlayable = false;  // VIP或需购买的歌曲
                }
            }
            
            // 只添加可播放的歌曲
            if (isPlayable) {
                m_songs.append(song);
                validCount++;
                
                // 添加到列表
                QString displayText = QString("🎵 %1\n👤 %2  |  💿 %3  |  ⏱️ %4:%5")
                    .arg(song.name)
                    .arg(song.artist)
                    .arg(song.album)
                    .arg(song.duration / 60)
                    .arg(song.duration % 60, 2, 10, QChar('0'));
                
                m_resultList->addItem(displayText);
            }
        }
        
        if (validCount == 0) {
            m_statusLabel->setText("未找到可播放的歌曲");
            showDemoResults();
        } else {
            m_statusLabel->setText(QString("找到 %1 首可播放歌曲").arg(validCount));
        }
    }
    
    // 显示演示数据（当API不可用时）
    void showDemoResults()
    {
        m_songs.clear();
        m_resultList->clear();
        
        // 添加一些示例歌曲（仅显示可播放的）
        QList<QPair<QString, QString>> demoSongs = {
            {"告白气球", "周杰伦"},
            {"晴天", "周杰伦"},
            {"稻香", "周杰伦"}
        };
        
        for (const auto& demo : demoSongs) {
            SongInfo song;
            song.name = demo.first;
            song.artist = demo.second;
            song.album = "示例专辑";
            song.duration = 240;
            // 使用网易云音乐外链（示例）
            song.url = "http://music.163.com/song/media/outer/url?id=25906124.mp3";
            
            m_songs.append(song);
            
            QString displayText = QString("🎵 %1\n👤 %2  |  💿 %3  |  ⏱️ %4:%5 | ✅ 可播放")
                .arg(song.name)
                .arg(song.artist)
                .arg(song.album)
                .arg(song.duration / 60)
                .arg(song.duration % 60, 2, 10, QChar('0'));
            
            m_resultList->addItem(displayText);
        }
        
        m_statusLabel->setText(QString("演示模式：显示 %1 首可播放歌曲（API暂不可用）").arg(m_songs.size()));
    }
    
    void onPlaySelected()
    {
        int row = m_resultList->currentRow();
        if (row < 0 || row >= m_songs.size()) {
            QMessageBox::warning(this, "提示", "请先选择一首歌曲！");
            return;
        }
        
        SongInfo song = m_songs[row];
        
        // 发送信号
        emit songSelected(song);
        
        // 关闭对话框
        accept();
    }
};

#endif // ONLINEMUSICSEARCH_H
