#ifndef PLAYHISTORY_H
#define PLAYHISTORY_H

#include <QObject>
#include <QVector>
#include <QString>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>

// 播放历史记录项
struct HistoryItem
{
    QString filePath;           // 文件路径
    QString fileName;           // 文件名
    QString fileType;           // 文件类型（audio/video）
    QDateTime lastPlayTime;     // 最后播放时间
    int playCount;              // 播放次数
    qint64 lastPosition;        // 最后播放位置（毫秒）
    qint64 duration;            // 文件时长（毫秒）
    
    HistoryItem()
        : playCount(0)
        , lastPosition(0)
        , duration(0)
    {}
    
    // 转换为JSON对象
    QJsonObject toJson() const
    {
        QJsonObject obj;
        obj["filePath"] = filePath;
        obj["fileName"] = fileName;
        obj["fileType"] = fileType;
        obj["lastPlayTime"] = lastPlayTime.toString(Qt::ISODate);
        obj["playCount"] = playCount;
        obj["lastPosition"] = QString::number(lastPosition);
        obj["duration"] = QString::number(duration);
        return obj;
    }
    
    // 从JSON对象创建
    static HistoryItem fromJson(const QJsonObject &obj)
    {
        HistoryItem item;
        item.filePath = obj["filePath"].toString();
        item.fileName = obj["fileName"].toString();
        item.fileType = obj["fileType"].toString();
        item.lastPlayTime = QDateTime::fromString(obj["lastPlayTime"].toString(), Qt::ISODate);
        item.playCount = obj["playCount"].toInt();
        item.lastPosition = obj["lastPosition"].toString().toLongLong();
        item.duration = obj["duration"].toString().toLongLong();
        return item;
    }
};

// 播放历史管理器
class PlayHistoryManager : public QObject
{
    Q_OBJECT
    
private:
    QVector<HistoryItem> m_history;     // 历史记录列表
    QString m_historyFilePath;          // 历史记录文件路径
    int m_maxHistoryCount;              // 最大历史记录数
    
public:
    explicit PlayHistoryManager(QObject *parent = nullptr)
        : QObject(parent)
        , m_maxHistoryCount(100)
    {
        // 设置历史记录文件路径
        QString dataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(dataPath);
        if (!dir.exists()) {
            dir.mkpath(dataPath);
        }
        m_historyFilePath = dataPath + "/play_history.json";
        
        // 加载历史记录
        loadHistory();
    }
    
    ~PlayHistoryManager()
    {
        saveHistory();
    }
    
    // 添加或更新播放记录
    void addOrUpdateHistory(const QString &filePath, const QString &fileType, 
                           qint64 position = 0, qint64 duration = 0)
    {
        QFileInfo fileInfo(filePath);
        if (!fileInfo.exists()) {
            return;
        }
        
        // 查找是否已存在
        int existingIndex = -1;
        for (int i = 0; i < m_history.size(); ++i) {
            if (m_history[i].filePath == filePath) {
                existingIndex = i;
                break;
            }
        }
        
        if (existingIndex >= 0) {
            // 更新现有记录
            HistoryItem &item = m_history[existingIndex];
            item.lastPlayTime = QDateTime::currentDateTime();
            item.playCount++;
            item.lastPosition = position;
            if (duration > 0) {
                item.duration = duration;
            }
            
            // 移到最前面
            if (existingIndex != 0) {
                m_history.move(existingIndex, 0);
            }
        } else {
            // 创建新记录
            HistoryItem item;
            item.filePath = filePath;
            item.fileName = fileInfo.fileName();
            item.fileType = fileType;
            item.lastPlayTime = QDateTime::currentDateTime();
            item.playCount = 1;
            item.lastPosition = position;
            item.duration = duration;
            
            // 插入到最前面
            m_history.prepend(item);
            
            // 限制历史记录数量
            if (m_history.size() > m_maxHistoryCount) {
                m_history.resize(m_maxHistoryCount);
            }
        }
        
        // 保存到文件
        saveHistory();
        
        emit historyUpdated();
    }
    
    // 获取所有历史记录
    QVector<HistoryItem> getHistory() const
    {
        return m_history;
    }
    
    // 获取指定类型的历史记录
    QVector<HistoryItem> getHistoryByType(const QString &fileType) const
    {
        QVector<HistoryItem> result;
        for (const auto &item : m_history) {
            if (item.fileType == fileType) {
                result.append(item);
            }
        }
        return result;
    }
    
    // 获取最近播放的N条记录
    QVector<HistoryItem> getRecentHistory(int count) const
    {
        QVector<HistoryItem> result;
        int n = qMin(count, m_history.size());
        for (int i = 0; i < n; ++i) {
            result.append(m_history[i]);
        }
        return result;
    }
    
    // 清除所有历史记录
    void clearHistory()
    {
        m_history.clear();
        saveHistory();
        emit historyUpdated();
    }
    
    // 删除指定的历史记录
    void removeHistory(const QString &filePath)
    {
        for (int i = 0; i < m_history.size(); ++i) {
            if (m_history[i].filePath == filePath) {
                m_history.removeAt(i);
                saveHistory();
                emit historyUpdated();
                break;
            }
        }
    }
    
    // 获取历史记录数量
    int getHistoryCount() const
    {
        return m_history.size();
    }
    
signals:
    void historyUpdated();
    
private:
    // 保存历史记录到文件
    void saveHistory()
    {
        QJsonArray jsonArray;
        for (const auto &item : m_history) {
            jsonArray.append(item.toJson());
        }
        
        QJsonObject root;
        root["version"] = "1.0";
        root["history"] = jsonArray;
        
        QJsonDocument doc(root);
        
        QFile file(m_historyFilePath);
        if (file.open(QIODevice::WriteOnly)) {
            file.write(doc.toJson());
            file.close();
        }
    }
    
    // 从文件加载历史记录
    void loadHistory()
    {
        QFile file(m_historyFilePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return;
        }
        
        QByteArray data = file.readAll();
        file.close();
        
        QJsonDocument doc = QJsonDocument::fromJson(data);
        if (!doc.isObject()) {
            return;
        }
        
        QJsonObject root = doc.object();
        QJsonArray jsonArray = root["history"].toArray();
        
        m_history.clear();
        for (const auto &value : jsonArray) {
            if (value.isObject()) {
                HistoryItem item = HistoryItem::fromJson(value.toObject());
                // 验证文件是否仍然存在
                if (QFileInfo::exists(item.filePath)) {
                    m_history.append(item);
                }
            }
        }
    }
};

#endif // PLAYHISTORY_H
