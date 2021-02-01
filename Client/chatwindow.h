#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include "asyncclient.h"
#include "asyncclientmanager.h"
#include <QMainWindow>
#include <QtWidgets>

namespace Ui {
class ChatWindow;
}

class ChatCreationDialog;

class ChatWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();
    friend class ChatCreationDialog;

private:
    Ui::ChatWindow *ui;
    AsyncClient *client;
    AsyncClientManager *clientManager;
    QJsonArray chats;
    QMap<size_t, QJsonArray> messagesInChats;

    int getChatIDByName(const QString&);

private slots:
    void slotUpdUsername(QString);
    void slotUpdChatList(QJsonArray);
    void openChat(QListWidgetItem*, QListWidgetItem*);
    void slotUpdNewestMessages(size_t, QJsonArray);

    void on_pushButtonSendMessage_released();

    void on_lineEditMessage_textChanged(const QString &arg1);

    void on_actionCreate_triggered();

signals:
    void stopClientManager();
    void startClientManager();
    void sendMessage(QString);
    void setCurrentChatID(int);
};

#endif // CHATWINDOW_H
