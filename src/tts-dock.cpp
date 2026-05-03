#include "tts-dock.hpp"
#include <obs-frontend-api.h>
#include <QMenu>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QPainter>

// --- VoiceDelegate Implementation ---
void VoiceDelegate::paint(QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index) const {
    QStyledItemDelegate::paint(painter, option, index);
    bool downloaded = index.data(Qt::UserRole + 1).toBool();
    int progress = index.data(Qt::UserRole + 2).toInt();
    bool selected = option.state & QStyle::State_Selected;
    painter->save();
    QRect rect = option.rect;
    if (downloaded) {
        painter->setPen(Qt::gray);
        painter->drawText(rect.adjusted(0,0,-5,0), Qt::AlignRight | Qt::AlignVCenter, "✓");
        if (selected) {
            painter->setPen(QColor("#53FC18"));
            painter->drawText(rect.adjusted(0,0,-15,0), Qt::AlignRight | Qt::AlignVCenter, "✓✓");
        }
    } else if (progress > 0 && progress < 100) {
        painter->setBrush(QColor("#53FC18"));
        painter->drawRect(rect.right() - 40, rect.top() + 5, (progress * 30) / 100, rect.height() - 10);
    }
    painter->restore();
}

// --- TTSDock Implementation ---
TTSDock::TTSDock(QWidget *parent) : QWidget(parent)
{
    networkManager = new QNetworkAccessManager(this);
    
    twitchSocket = new QWebSocket();
    connect(twitchSocket, &QWebSocket::connected, this, &TTSDock::OnTwitchConnected);
    connect(twitchSocket, &QWebSocket::textMessageReceived, this, &TTSDock::OnTwitchMessageReceived);
    connect(twitchSocket, &QWebSocket::disconnected, this, &TTSDock::OnTwitchDisconnected);
    connect(twitchSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &TTSDock::OnTwitchError);

    kickSocket = new QWebSocket();
    connect(kickSocket, &QWebSocket::connected, this, &TTSDock::OnKickConnected);
    connect(kickSocket, &QWebSocket::textMessageReceived, this, &TTSDock::OnKickMessageReceived);
    connect(kickSocket, &QWebSocket::disconnected, this, &TTSDock::OnKickDisconnected);
    connect(kickSocket, QOverload<QAbstractSocket::SocketError>::of(&QWebSocket::errorOccurred), this, &TTSDock::OnKickError);

    refreshTimer = new QTimer(this);
    connect(refreshTimer, &QTimer::timeout, this, &TTSDock::RefreshVoices);
    refreshTimer->start(2000);

    connect(networkManager, &QNetworkAccessManager::finished, this, &TTSDock::OnReplyFinished);

    SetupUI();
    RefreshVoices();
}

void TTSDock::SetupUI()
{
    mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    // Settings
    settingsGroup = new QGroupBox("Hardware & AI Engine");
    QVBoxLayout *settingsLayout = new QVBoxLayout();
    QHBoxLayout *selectors = new QHBoxLayout();
    engineSelector = new QComboBox();
    engineSelector->addItems({"Piper (Local)", "Kokoro (Premium)"});
    backendSelector = new QComboBox();
    backendSelector->addItems({"Vulkan (Any GPU)", "ROCm (AMD)"});
    selectors->addWidget(engineSelector);
    selectors->addWidget(backendSelector);
    settingsLayout->addLayout(selectors);

    QHBoxLayout *voiceRow = new QHBoxLayout();
    voiceSelector = new QComboBox();
    voiceSelector->setItemDelegate(new VoiceDelegate(this));
    downloadButton = new QPushButton("Download");
    voiceRow->addWidget(voiceSelector, 1);
    voiceRow->addWidget(downloadButton);
    settingsLayout->addLayout(voiceRow);
    settingsGroup->setLayout(settingsLayout);
    mainLayout->addWidget(settingsGroup);

    // Stream Integration
    streamGroup = new QGroupBox("Channel Integration");
    QVBoxLayout *streamLayout = new QVBoxLayout();
    twitchInput = new QLineEdit();
    twitchInput->setPlaceholderText("Twitch Channel");
    kickInput = new QLineEdit();
    kickInput->setPlaceholderText("Kick Channel");
    connectButton = new QPushButton("Connect Streams");
    streamLayout->addWidget(twitchInput);
    streamLayout->addWidget(kickInput);
    streamLayout->addWidget(connectButton);
    streamGroup->setLayout(streamLayout);
    mainLayout->addWidget(streamGroup);

    connect(connectButton, &QPushButton::clicked, this, &TTSDock::ToggleChatConnection);
    connect(downloadButton, &QPushButton::clicked, this, &TTSDock::OnVoiceDownloadClicked);

    // Chat Preview
    chatPreview = new QTextEdit();
    chatPreview->setReadOnly(true);
    chatPreview->setStyleSheet("background-color: #1a1a1a; color: white; font-family: 'Segoe UI', sans-serif;");
    mainLayout->addWidget(chatPreview);

    // Simulation
    simGroup = new QGroupBox("Simulation Box");
    QHBoxLayout *simLayout = new QHBoxLayout();
    testInput = new QLineEdit();
    testButton = new QPushButton("Speak");
    connect(testButton, &QPushButton::clicked, this, &TTSDock::HandleTestTTS);
    simLayout->addWidget(testInput);
    simLayout->addWidget(testButton);
    simGroup->setLayout(simLayout);
    mainLayout->addWidget(simGroup);

    setLayout(mainLayout);
}

void TTSDock::ToggleChatConnection()
{
    if (isConnected) {
        DisconnectChat();
    } else {
        if (!twitchInput->text().isEmpty()) ConnectTwitch(twitchInput->text().trimmed());
        if (!kickInput->text().isEmpty()) ConnectKick(kickInput->text().trimmed());
        isConnected = true;
        connectButton->setText("Stop Integration");
    }
}

void TTSDock::ConnectTwitch(const QString &channel)
{
    twitchSocket->open(QUrl("wss://irc-ws.chat.twitch.tv:443"));
    chatPreview->append("<i style='color: #9146FF;'>Connecting to Twitch...</i>");
}

void TTSDock::OnTwitchConnected()
{
    twitchSocket->sendTextMessage("CAP REQ :twitch.tv/tags");
    twitchSocket->sendTextMessage("PASS SCHMOOPIIE");
    twitchSocket->sendTextMessage("NICK justinfan12345");
    twitchSocket->sendTextMessage("JOIN #" + twitchInput->text().trimmed().toLower());
    chatPreview->append("<i style='color: #9146FF;'>Twitch Chat Connected!</i>");
}

void TTSDock::ConnectKick(const QString &channel)
{
    // Kick uses Pusher. This is a simplified connection to the Pusher app Kick uses.
    // Real implementation usually needs a room ID, but some channels use the name.
    kickSocket->open(QUrl("wss://ws-us2.pusher.com/app/eb1d5f283081a78b93bb?protocol=7&client=js&version=7.4.0"));
    chatPreview->append("<i style='color: #53FC18;'>Connecting to Kick...</i>");
}

void TTSDock::OnKickConnected()
{
    // Pusher requires a subscription message
    // Note: This logic assumes we know the room ID or can subscribe via channel name
    // For now, we subscribe to a placeholder. Real Kick API call would fetch room ID first.
    chatPreview->append("<i style='color: #53FC18;'>Kick Socket Established.</i>");
}

void TTSDock::OnTwitchMessageReceived(const QString &message)
{
    if (message.startsWith("PING")) {
        twitchSocket->sendTextMessage("PONG :tmi.twitch.tv");
        return;
    }

    static QRegularExpression msgRegex("@([^ ]+) :[^ ]+ PRIVMSG #[^ ]+ :(.+)");
    QRegularExpressionMatch match = msgRegex.match(message);

    if (match.hasMatch()) {
        QString tags = match.captured(1);
        QString fullMsg = match.captured(2);
        
        QString ttsMsg = fullMsg;
        QString htmlMsg = fullMsg;

        // Extract display name from tags
        static QRegularExpression nameRegex("display-name=([^; ]+)");
        QRegularExpressionMatch nameMatch = nameRegex.match(tags);
        QString username = nameMatch.hasMatch() ? nameMatch.captured(1) : "TwitchUser";

        // Parse emotes
        static QRegularExpression emotesTagRegex("emotes=([^; ]+)");
        QRegularExpressionMatch emotesMatch = emotesTagRegex.match(tags);
        if (emotesMatch.hasMatch()) {
            htmlMsg = ParseTwitchEmotes(fullMsg, emotesMatch.captured(1), ttsMsg);
        }

        ProcessChatMessage(username, htmlMsg, "#9146FF", "Twitch");
        SendToTTS(ttsMsg);
    }
}

QString TTSDock::ParseTwitchEmotes(const QString &message, const QString &emotesTag, QString &ttsText)
{
    // Tag format: 25:0-4,12-16/1902:6-10
    QString html = message;
    QStringList emoteList = emotesTag.split('/');
    
    // We process in reverse to keep indices valid if we were replacing text,
    // but here we'll just replace substrings.
    for (const QString &emote : emoteList) {
        QString id = emote.split(':')[0];
        QString positions = emote.split(':')[1];
        QString firstPos = positions.split(',')[0];
        int start = firstPos.split('-')[0].toInt();
        int end = firstPos.split('-')[1].toInt();
        
        QString emoteText = message.mid(start, end - start + 1);
        QString imgTag = QString("<img src='https://static-cdn.jtvnw.net/emoticons/v2/%1/default/dark/1.0' width='24' height='24' />").arg(id);
        
        html.replace(emoteText, imgTag);
        ttsText.replace(emoteText, ""); // Remove from TTS
    }
    return html;
}

void TTSDock::OnKickMessageReceived(const QString &message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    QJsonObject obj = doc.object();
    QString event = obj["event"].toString();

    if (event == "App\\Events\\ChatMessageEvent") {
        QJsonObject data = QJsonDocument::fromJson(obj["data"].toString().toUtf8()).object();
        QString username = data["sender"].toObject()["username"].toString();
        QString content = data["content"].toString();
        
        QString ttsMsg = content;
        // Kick uses [emote:id:name] format or similar
        static QRegularExpression kickEmote("\\[emote:(\\d+):([^\\]]+)\\]");
        QString htmlMsg = content;
        htmlMsg.replace(kickEmote, "<img src='https://files.kick.com/emotes/\\1/full' width='24' height='24' />");
        ttsMsg.replace(kickEmote, "");

        ProcessChatMessage(username, htmlMsg, "#53FC18", "Kick");
        SendToTTS(ttsMsg);
    } else if (event == "pusher:connection_established") {
        // Now subscribe to the chatroom (Ideally we fetch room ID from Kick API first)
        // For demonstration, we assume a room ID or subscription logic here
    }
}

void TTSDock::ProcessChatMessage(const QString &username, const QString &message, const QString &color, const QString &platform)
{
    QString html = QString("<span style='color: %1;'><b>[%2] %3:</b></span> %4").arg(color, platform, username, message);
    chatPreview->append(html);
}

void TTSDock::SendToTTS(const QString &text)
{
    if (text.trimmed().isEmpty()) return;
    QJsonObject json;
    json["text"] = text;
    json["engine"] = engineSelector->currentText();
    json["backend"] = backendSelector->currentText().contains("ROCm") ? "ROCm" : "Vulkan";
    json["voice"] = voiceSelector->currentData().toString();
    
    QNetworkRequest request(QUrl("http://127.0.0.1:6973/tts"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    networkManager->post(request, QJsonDocument(json).toJson());
}

// Boilerplate/Stubs
void TTSDock::DisconnectChat() { twitchSocket->close(); kickSocket->close(); isConnected = false; connectButton->setText("Connect Streams"); }
void TTSDock::RefreshVoices() { networkManager->get(QNetworkRequest(QUrl("http://127.0.0.1:6973/voices"))); }
void TTSDock::OnVoiceDownloadClicked() { 
    QNetworkRequest request(QUrl("http://127.0.0.1:6973/download_voice/" + voiceSelector->currentData().toString()));
    networkManager->post(request, QByteArray()); 
}
void TTSDock::HandleTestTTS() { ProcessChatMessage("Tester", testInput->text(), "#ffffff", "Sim"); SendToTTS(testInput->text()); testInput->clear(); }
void TTSDock::OnTwitchDisconnected() {}
void TTSDock::OnTwitchError(QAbstractSocket::SocketError) {}
void TTSDock::OnKickDisconnected() {}
void TTSDock::OnKickError(QAbstractSocket::SocketError) {}
void TTSDock::OnReplyFinished(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) return;

    QUrl url = reply->url();
    if (url.path() == "/voices") {
        QByteArray data = reply->readAll();
        QJsonDocument doc = QJsonDocument::fromJson(data);
        QJsonArray voices = doc.array();

        QString currentVoice = voiceSelector->currentData().toString();
        
        // Temporarily block signals to avoid triggering selection changes while rebuilding
        voiceSelector->blockSignals(true);
        voiceSelector->clear();
        
        for (int i = 0; i < voices.size(); ++i) {
            QJsonObject voice = voices[i].toObject();
            QString id = voice["id"].toString();
            QString name = voice["display_name"].toString();
            bool downloaded = voice["downloaded"].toBool();
            int progress = voice["progress"].toInt();

            voiceSelector->addItem(name, id);
            int idx = voiceSelector->count() - 1;
            voiceSelector->setItemData(idx, downloaded, Qt::UserRole + 1);
            voiceSelector->setItemData(idx, progress, Qt::UserRole + 2);

            if (id == currentVoice) {
                voiceSelector->setCurrentIndex(idx);
            }
        }
        
        // If nothing was selected and we have voices, select the first one (usually English/Downloaded)
        if (voiceSelector->currentIndex() == -1 && voiceSelector->count() > 0) {
            voiceSelector->setCurrentIndex(0);
        }

        voiceSelector->blockSignals(false);
        
        // Update download button state
        bool currentDownloaded = voiceSelector->currentData(Qt::UserRole + 1).toBool();
        downloadButton->setEnabled(!currentDownloaded);
        downloadButton->setText(currentDownloaded ? "Downloaded" : "Download");
    }
}
void TTSDock::OnSimulationToggled(bool) {}
void TTSDock::InsertEmoji() {}

void RegisterTTSDock() { obs_frontend_add_dock_by_id("OmniversifyTTS", "Omniversify TTS", new TTSDock()); }
