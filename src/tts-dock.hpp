#pragma once

#include <obs-module.h>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QCheckBox>
#include <QTextEdit>
#include <QGroupBox>
#include <QTimer>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkRequest>
#include <QtNetwork/QNetworkReply>
#include <QtWebSockets/QWebSocket>
#include <QStyledItemDelegate>

// Delegate to draw voice items with download status
class VoiceDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const override;
};

class TTSDock : public QWidget {
    Q_OBJECT

public:
    TTSDock(QWidget *parent = nullptr);

private slots:
    void HandleTestTTS();
    void OnSimulationToggled(bool checked);
    void InsertEmoji();
    void OnReplyFinished(QNetworkReply *reply);
    
    // Twitch slots
    void OnTwitchConnected();
    void OnTwitchMessageReceived(const QString &message);
    void OnTwitchDisconnected();
    void OnTwitchError(QAbstractSocket::SocketError error);

    // Kick slots
    void OnKickConnected();
    void OnKickMessageReceived(const QString &message);
    void OnKickDisconnected();
    void OnKickError(QAbstractSocket::SocketError error);

    // Common slots
    void ToggleChatConnection();
    void RefreshVoices();
    void OnVoiceDownloadClicked();

private:
    QVBoxLayout *mainLayout;
    
    // Engine & Backend Selection
    QGroupBox *settingsGroup;
    QComboBox *engineSelector;
    QComboBox *backendSelector;
    QComboBox *voiceSelector;
    QPushButton *downloadButton;

    // Stream Integration
    QGroupBox *streamGroup;
    QLineEdit *twitchInput;
    QLineEdit *kickInput;
    QPushButton *connectButton;

    // Simulation Mode
    QGroupBox *simGroup;
    QCheckBox *simulationToggle;
    QLineEdit *testInput;
    QPushButton *testButton;
    QPushButton *emojiButton;

    // Chat Preview
    QTextEdit *chatPreview;

    // Networking
    QNetworkAccessManager *networkManager;
    QWebSocket *twitchSocket;
    QWebSocket *kickSocket;
    QTimer *refreshTimer;
    bool isConnected = false;

    void SetupUI();
    void ConnectTwitch(const QString &channel);
    void ConnectKick(const QString &channel);
    void DisconnectChat();
    
    void ProcessChatMessage(const QString &username, const QString &message, const QString &color, const QString &platform);
    void SendToTTS(const QString &text);
    
    QString ParseTwitchEmotes(const QString &message, const QString &emotesTag, QString &ttsText);
    QString ParseKickEmotes(const QString &message, QString &ttsText);
};

void RegisterTTSDock();
