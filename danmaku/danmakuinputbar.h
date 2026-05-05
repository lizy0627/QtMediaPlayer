#ifndef DANMAKUINPUTBAR_H
#define DANMAKUINPUTBAR_H

#include <QString>
#include <QWidget>

struct SessionState;
class QComboBox;
class QEvent;
class QLineEdit;
class QPushButton;
class UserSession;

class DanmakuInputBar : public QWidget
{
    Q_OBJECT

public:
    explicit DanmakuInputBar(QWidget* parent = nullptr);

    void setUserSession(UserSession* session);
    QString currentUser() const;
    bool isLoggedIn() const;

public slots:
    void setCurrentUser(const QString& username);
    void setSessionState(const SessionState& state);

signals:
    void danmakuSubmitted(const QString& content, const QString& color, int type);
    void loginRequired();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void createUi();
    void connectSignals();
    void updateUi();
    void updateColorButtonStyle();
    void onSendClicked();
    void onColorButtonClicked();

    QLineEdit* m_inputEdit = nullptr;
    QPushButton* m_sendButton = nullptr;
    QPushButton* m_colorButton = nullptr;
    QComboBox* m_typeCombo = nullptr;
    QString m_selectedColor = QStringLiteral("#FFFFFF");
    QString m_currentUser;
    UserSession* m_userSession = nullptr;
};

#endif // DANMAKUINPUTBAR_H
