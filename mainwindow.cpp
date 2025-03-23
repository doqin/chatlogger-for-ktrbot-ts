#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow),
    networkManager(new QNetworkAccessManager(this)) {
    ui->setupUi(this);

    qDebug() << QSslSocket::supportsSsl();
    qDebug() << QSslSocket::sslLibraryBuildVersionString();
    qDebug() << QSslSocket::sslLibraryVersionString();

    try {
        loadSettings();
        connect(ui->refreshButton, &QPushButton::clicked, this,
                &MainWindow::fetchChannels);
        connect(ui->channelList, &QListWidget::itemClicked, this,
                &MainWindow::fetchMessages);
        fetchChannels();
    } catch (const std::exception &e) {
        QString errorMessage = QString("Error: %1").arg(e.what());
        QMessageBox::information(parent, "Error", errorMessage);
        qDebug() << "Error:" << e.what();
        return;
    }
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

    ui->channelList->clear();
    for (const QJsonValue &value : std::as_const(channels)) {
        QJsonObject obj = value.toObject();
        QString channelId = obj["id"].toString();
        QString channelName = obj["name"].toString();

        QListWidgetItem *item = new QListWidgetItem(channelName);
        item->setData(Qt::UserRole, channelId);
        ui->channelList->addItem(item);
    }

    reply->deleteLater();
}

void MainWindow::fetchMessages() {
    QListWidgetItem *item = ui->channelList->currentItem();
    if (!item)
        return;

    QString channelId = item->data(Qt::UserRole).toString();
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

    reply->deleteLater();
}
