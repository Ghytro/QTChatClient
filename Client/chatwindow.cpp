#include "chatwindow.h"
#include "ui_chatwindow.h"
#include "chatcreationdialog.h"

ChatWindow::ChatWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::ChatWindow)
{
    ui->setupUi(this);
    connect(this->ui->listWidgetChats, SIGNAL(currentItemChanged(QListWidgetItem*, QListWidgetItem*)),
            this,                      SLOT(openChat(QListWidgetItem*, QListWidgetItem*)));

    //setting up tcp client manager
    //in a seperate thread
    //in order not to freeze main thread
    client = new AsyncClient(9999);

    connect(client, SIGNAL(updUsername(QString)),
            this,   SLOT(slotUpdUsername(QString)));

    connect(client, SIGNAL(updChatList(QJsonArray)),
            this,   SLOT(slotUpdChatList(QJsonArray)));

    connect(client, SIGNAL(updNewestMessages(size_t, QJsonArray)),
            this,   SLOT(slotUpdNewestMessages(size_t, QJsonArray)));

    clientManager = new AsyncClientManager(client);
    QThread *thread = new QThread;
    clientManager->moveToThread(thread);

    connect(thread,        SIGNAL(started()),
            clientManager, SLOT(start()));

    connect(clientManager, SIGNAL(stopped()),
            thread,        SLOT(quit()));

    connect(this,          SIGNAL(stopClientManager()),
            clientManager, SLOT(stop()));

    connect(this,          SIGNAL(setCurrentChatID(int)),
            clientManager, SLOT(slotSetCurrentChatID(int)));

    connect(clientManager, SIGNAL(stopped()),
            clientManager, SLOT(deleteLater()));

    connect(thread,        SIGNAL(finished()),
            thread,        SLOT(deleteLater()));

    thread->start();
}

ChatWindow::~ChatWindow()
{
    delete ui;
    client->deleteLater();
}

void ChatWindow::slotUpdUsername(QString username)
{
    this->ui->labelUsername->setText(username);
}

void ChatWindow::slotUpdChatList(QJsonArray arr)
{
    if (arr == this->chats)
        return;

    qDebug() << "ChatList updated";

    this->chats = arr;
    this->ui->listWidgetChats->blockSignals(true);
    this->ui->listWidgetChats->clear();
    this->ui->listWidgetChats->blockSignals(false);
    for (QJsonValue i: this->chats)
        this->ui->listWidgetChats->addItem(i["name"].toString());
}

int ChatWindow::getChatIDByName(const QString &name)
{
    for (QJsonValue i: this->chats)
        if (i["name"].toString() == name)
            return i["id"].toInt();
    return -1;
}

void ChatWindow::openChat(QListWidgetItem *current, QListWidgetItem *previous)
{
    int chatID = this->getChatIDByName(current->text());

    if (chatID == -1)
        return;
    this->ui->listWidgetMessages->blockSignals(true);
    this->ui->listWidgetMessages->clear();
    this->ui->listWidgetMessages->blockSignals(false);
    this->ui->lineEditMessage->setEnabled(true);

    this->messagesInChats[chatID] = QJsonArray();
    emit setCurrentChatID(chatID);
}

void ChatWindow::slotUpdNewestMessages(size_t chatID, QJsonArray latestMessages)
{
    //getting actual info to data
    int rightOffset = latestMessages.size();
    if (this->messagesInChats[chatID].size() != 0)
    {
        int serverLastMessageID = latestMessages.last().toObject()["id"].toInt();
        int localLastMessageID  = this->messagesInChats[chatID].last().toObject()["id"].toInt();

        rightOffset = serverLastMessageID - localLastMessageID;
    }
    bool scrollDown = false;
    if (rightOffset != 0) //got any elements to add
    {
        QScrollBar *scrollbar = this->ui->listWidgetMessages->verticalScrollBar();
        if (scrollbar->value() == scrollbar->maximum()) //the slider is at the end
            scrollDown = true;
    }
    for (int i = latestMessages.size() - rightOffset; i < latestMessages.size(); ++i)
    {
        this->messagesInChats[chatID].append(latestMessages[i]);

        QString messageSender = latestMessages[i].toObject()["sender_username"].toString();
        if (latestMessages[i].toObject()["type"] != QJsonValue::Null)
            messageSender = "SystemBot";

        this->ui->listWidgetMessages->addItem(QStringLiteral("<%1> %2 says: %3")
                                              .arg(latestMessages[i].toObject()["date"].toString())
                                              .arg(messageSender)
                                              .arg(latestMessages[i].toObject()["text"].toString()));
    }
    if (scrollDown)
        this->ui->listWidgetMessages->scrollToBottom();
}

void ChatWindow::on_pushButtonSendMessage_released()
{
    QString messageText = this->ui->lineEditMessage->text();
    this->ui->lineEditMessage->clear();
    this->clientManager->addPendingMessage(messageText);
}

void ChatWindow::on_lineEditMessage_textChanged(const QString &arg1)
{
    this->ui->pushButtonSendMessage->setEnabled(arg1.length() != 0);
}

void ChatWindow::on_actionCreate_triggered()
{
    this->clientManager->stop();
    ChatCreationDialog ccdialog(this);
    ccdialog.setModal(true);
    ccdialog.exec();
    this->clientManager->start();
}
