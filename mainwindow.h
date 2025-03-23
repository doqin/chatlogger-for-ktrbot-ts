#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QNetworkAccessManager>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void loadSettings();
    void saveSettings();

    void fetchChannels();
    void handleChannelResponse();
    void fetchMessages();
    void handleMessageResponse();

private:
    QNetworkAccessManager *networkManager;
    Ui::MainWindow *ui;
    int serverPort;
};
#endif // MAINWINDOW_H
