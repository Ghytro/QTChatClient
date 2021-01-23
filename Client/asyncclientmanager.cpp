#include "asyncclientmanager.h"
#include "asyncclient.h"

AsyncClientManager::AsyncClientManager(AsyncClient *client)
{
    this->client = client;
    connect(this,         SIGNAL(sendDataFromClient(QByteArray)),
            this->client, SLOT(sendData(QByteArray)));

    connect(this->client, SIGNAL(gotReply()),
            this,         SLOT(start()));
}

AsyncClientManager::~AsyncClientManager()
{

}

void AsyncClientManager::slotSetCurrentChatID(int chatID)
{
    this->currentChatID = chatID;
}

void AsyncClientManager::start()
{
    //getting user info & verifying token
    //add feature if token is incorrect then
    //open dialog with error and send to auth screen
    QFile tokenFile("token");
    if (!tokenFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open file with tokens";
        return;
    }
    QString token = tokenFile.readAll();
    tokenFile.close();
    QJsonObject query;
    query.insert("method", "user.getmyinfo");
    QJsonObject params;
    params.insert("access_token", QJsonValue::fromVariant(token));
    params.insert("current_chat_id", QJsonValue::fromVariant(this->currentChatID));
    params.insert("messages_num", QJsonValue::fromVariant(AsyncClientManager::latestMessagesNum));
    if (!this->pendingMessages.empty())
    {
        size_t chatID = this->pendingMessages.keys().first();
        QString messageText = this->pendingMessages[chatID].dequeue();

        if (this->pendingMessages[chatID].empty())
            this->pendingMessages.remove(chatID);

        QJsonObject messageToSend;
        messageToSend.insert("text", QJsonValue::fromVariant(messageText));
        messageToSend.insert("chat_id", QJsonValue::fromVariant(chatID));
        params.insert("message_to_send", QJsonValue::fromVariant(messageToSend));
    }
    query.insert("params", QJsonValue::fromVariant(params));
    emit sendDataFromClient(QJsonDocument(query).toJson());
    QThread::msleep(100);
}

void AsyncClientManager::stop()
{
    this->isWorking = false;
}

void AsyncClientManager::addPendingMessage(QString messageText)
{
    qDebug() << "Message added";
    this->pendingMessages[this->currentChatID].enqueue(messageText);
}
