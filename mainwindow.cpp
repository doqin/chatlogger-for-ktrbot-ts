#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QNetworkReply>
#include <QNetworkRequest>

QTreeWidgetItem *findFolderByData(QTreeWidget *tree, const QVariant &value) {
    for (int i = 0; i < tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = tree->topLevelItem(i);
        if (item->data(0, Qt::UserRole) == value) {
            return item; // Found folder
        }
    }
    return nullptr; // Not found
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    networkManager(new QNetworkAccessManager(this)),
    webSocket(new QWebSocket) {
    ui->setupUi(this);

    loadSettings();
    connect(ui->refreshButton, &QPushButton::clicked, this,
            &MainWindow::fetchServers);

    connect(ui->dmList, &QListWidget::itemClicked, this,
            &MainWindow::fetchMessages);

    connect(ui->serverList, &QTreeWidget::itemClicked, this,
            &MainWindow::fetchMessages);

    connect(webSocket, &QWebSocket::errorOccurred, this,
            &MainWindow::onErrorOccurred);
    connect(webSocket, &QWebSocket::connected, this, &MainWindow::onConnected);
    connect(webSocket, &QWebSocket::textMessageReceived, this,
            &MainWindow::onTextMessageReceived);
    connect(webSocket, &QWebSocket::disconnected, this,
            &MainWindow::onDisconnected);

    wsUrl = QString("ws://localhost:%1").arg(serverPort);

    qDebug() << "Connecting to webSocket at:" << wsUrl;
    webSocket->open(QUrl(wsUrl));

    fetchServers();
}

MainWindow::~MainWindow() { delete ui; }

void MainWindow::loadSettings() {
    QFile file("configurations.json");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray jsonData = file.readAll();
        file.close();

        QJsonDocument doc = QJsonDocument::fromJson(jsonData);
        QJsonObject obj = doc.object();

        serverPort = obj.value("port").toInt(8080); // Default to 8080 if missing
        ui->portInput->setText(QString::number(serverPort));
    } else {
        serverPort = 8080;
        ui->portInput->setText("8080");
    }
}

void MainWindow::saveSettings() {
    serverPort = ui->portInput->text().toInt();

    QJsonObject obj;
    obj["port"] = serverPort;

    QJsonDocument doc(obj);
    QFile file("configurations.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        ui->statusLabel->setText("Port saved!");
    } else {
        ui->statusLabel->setText("Failed to save port!");
    }
}

void MainWindow::fetchServers() {
    QString url = QString("http://localhost:%1/api/servers").arg(serverPort);
    QNetworkRequest request((QUrl(url)));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this,
            &MainWindow::handleServerResponse);
}

void MainWindow::handleServerResponse() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching servers:" << reply->errorString();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray servers = doc.array();

    ui->serverList->clear();
    for (const QJsonValue &value : std::as_const(servers)) {
        QJsonObject obj = value.toObject();
        QString serverId = obj["id"].toString();
        QString serverName = obj["name"].toString();

        QTreeWidgetItem *item = new QTreeWidgetItem(ui->serverList);
        item->setText(0, serverName);
        item->setData(0, Qt::UserRole, serverId);

        ui->serverList->expandItem(item);
    }

    reply->deleteLater();
    fetchChannels();
}

void MainWindow::fetchChannels() {
    QString url = QString("http://localhost:%1/api/channels").arg(serverPort);
    QNetworkRequest request((QUrl(url)));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this,
            &MainWindow::handleChannelResponse);
}

void MainWindow::handleChannelResponse() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching channels:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray channels = doc.array();

    ui->dmList->clear();
    for (const QJsonValue &value : std::as_const(channels)) {
        QJsonObject obj = value.toObject();
        QString channelId = obj["id"].toString();
        QString channelName = obj["name"].toString();
        QString serverId = obj["server_id"].toString();
        bool isDM = obj["is_dm"].toInt() != 0;

        if (isDM) {
            QListWidgetItem *item = new QListWidgetItem(channelName);
            item->setData(Qt::UserRole, channelId);
            ui->dmList->addItem(item);
        } else {
            QTreeWidgetItem *folder = findFolderByData(ui->serverList, serverId);
            if (folder) {
                qDebug() << folder->data(0, Qt::UserRole);
            } else {
                qDebug() << "Folder doesn't exist";
            }

            QTreeWidgetItem *item = new QTreeWidgetItem(folder);
            item->setText(0, channelName);
            item->setData(0, Qt::UserRole, channelId);
        }
    }

    reply->deleteLater();
}

void MainWindow::fetchMessages() {
    QString channelId;
    if (ui->lists->currentIndex() == 0) {
        QListWidgetItem *item = ui->dmList->currentItem();
        if (!item)
            return;

        channelId = item->data(Qt::UserRole).toString();
    } else {
        QTreeWidgetItem *item = ui->serverList->currentItem();
        if (!item)
            return;

        if (item->parent()) {
            channelId = item->data(0, Qt::UserRole).toString();
        } else {
            return;
        }
    }
    QString url = QString("http://localhost:%1/api/messages/").arg(serverPort);
    QNetworkRequest request((QUrl(url + channelId)));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this,
            &MainWindow::handleMessageResponse);
}

void MainWindow::handleMessageResponse() {
    QNetworkReply *reply = qobject_cast<QNetworkReply *>(sender());
    if (reply->error() != QNetworkReply::NoError) {
        qDebug() << "Error fetching messages:" << reply->errorString();
        reply->deleteLater();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
    QJsonArray messages = doc.array();

    ui->messageList->clear();
    for (int i = messages.size() - 1; i >= 0; i--) {
        QJsonObject obj = messages[i].toObject();
        QString username = obj["username"].toString();
        QString content = obj["content"].toString();
        ui->messageList->addItem(username + ": " + content);
    }

    ui->messageList->scrollToItem(ui->messageList->item(ui->messageList->count() - 1));
    reply->deleteLater();
}

void MainWindow::onErrorOccurred(QAbstractSocket::SocketError error) {
    qDebug() << "WebSocket Error:" << webSocket->errorString();
}

void MainWindow::onConnected() { qDebug() << "WebSocket connected!"; }

void MainWindow::onTextMessageReceived(const QString &message) {
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject())
        return;

    QJsonObject msgObj = doc.object();
    qDebug() << "New message received from server:";
    qDebug() << "User:" << msgObj["user_id"].toString();
    qDebug() << "Channel:" << msgObj["channel_id"].toString();
    qDebug() << "Content:" << msgObj["content"].toString();

    QString channelId;
    if (ui->lists->currentIndex() == 0) {
        QListWidgetItem *item = ui->dmList->currentItem();
        if (!item) {
            qDebug() << "Message does not belong to the current channel, fetching servers...";
            fetchServers();
            return;
        }

        channelId = item->data(Qt::UserRole).toString();
    } else {
        QTreeWidgetItem *item = ui->serverList->currentItem();
        if (!item) {
            qDebug() << "Message does not belong to the current channel, fetching servers...";
            fetchServers();
            return;
        }

        if (item->parent()) {
            channelId = item->data(0, Qt::UserRole).toString();
        } else {
            qDebug() << "Message does not belong to the current channel, fetching servers...";
            fetchServers();
            return;
        }
    }

    if (msgObj["channel_id"].toString() == channelId) {
        QString username = msgObj["username"].toString();
        QString content = msgObj["content"].toString();
        ui->messageList->addItem(username + ": " + content);
        ui->messageList->scrollToItem(ui->messageList->item(ui->messageList->count() - 1));
    } else {
        qDebug() << "Message does not belong to the current channel, fetching servers...";
        fetchServers(); // Temporary solution
    }
}

void MainWindow::onDisconnected() { qDebug() << "WebSocket disconnected!"; }

void MainWindow::sendMessage(const QString &channelId, const QString &message) {
    if (webSocket->isValid()) {
        QJsonObject messageJson;
        messageJson["channel_id"] = channelId;
        messageJson["content"] = message;

        QJsonDocument doc(messageJson);
        webSocket->sendTextMessage(doc.toJson(QJsonDocument::Compact));
    } else {
        qDebug() << "WebSocket is not connected!";
    }
}
