#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>
#include <QWebSocket>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // Settings methods
    void loadSettings();
    void saveSettings();

    // Fetching methods
    void fetchChannels();
    void handleChannelResponse();
    void fetchMessages();
    void handleMessageResponse();

    // WebSocket methods
    void onErrorOccurred(QAbstractSocket::SocketError);
    void onConnected();
    void onTextMessageReceived(const QString&);
    void onDisconnected();
    void sendMessage(const QString&, const QString&);

private:
    QNetworkAccessManager *networkManager;
    QWebSocket *webSocket;
    Ui::MainWindow *ui;
    int serverPort;
    QString wsUrl;
};
#endif // MAINWINDOW_H
