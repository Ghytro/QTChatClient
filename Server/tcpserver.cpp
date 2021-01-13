#include "tcpserver.h"
#include "exceptions.h"

Server::Server(quint16 port)
{
    this->server = new QTcpServer;
    if (!this->server->listen(QHostAddress::LocalHost, port))
    {
        qDebug() << "Unable to listen port" << port;
        return;
    }

    connect(this->server, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    qDebug() << "Server started";
}

Server::~Server()
{
    this->server->deleteLater();
}

void Server::slotNewConnection()
{
    QTcpSocket *clientSocket = this->server->nextPendingConnection();
    connect(clientSocket, SIGNAL(readyRead()), this, SLOT(slotReadClient()));
    connect(clientSocket, SIGNAL(disconnected()), clientSocket, SLOT(deleteLater()));
}

void Server::slotReadClient()
{
    const quint16 maxDataSize = 2048;
    QTcpSocket *clientSocket = static_cast<QTcpSocket*>(sender());
    if (clientSocket->bytesAvailable() > maxDataSize)
    {
        clientSocket->write("Query is too big");
        return;
    }
    QString reply = Server::parseQuery(clientSocket->readAll());
    clientSocket->write(reply.toUtf8());
}

void Server::createChat(const QString&             chatName,
                           const QJsonArray&       membersIDs,
                           const size_t&           adminID,
                           const bool&             isVisible)
{
    QDir().mkdir("chats");
    size_t chatID = QDir("chats").count() - 2;
    QDir("chats").mkdir(QString::number(chatID));

    QJsonObject json;
    json.insert("chat_name",        QJsonValue::fromVariant(chatName));
    json.insert("members",          QJsonValue::fromVariant(membersIDs));
    json.insert("admin",            QJsonValue::fromVariant(adminID));
    json.insert("is_visible",       QJsonValue::fromVariant(isVisible));
    json.insert("total_messages",   QJsonValue::fromVariant(0));

    QJsonDocument jsonDoc(json);
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    infoFile.open(QFile::WriteOnly);
    infoFile.write(jsonDoc.toJson());
    //qDebug() << "Chat successfully created";
}

void Server::sendMessage(const size_t&           chatID,
                            const QString&          messageText,
                            const size_t&           senderID)
{
    if (Server::isMemberOfChat(senderID, chatID))
    {
        qDebug() << "Can't send message: user" << senderID << "is not member of chat" << chatID;
        return;
    }
    //getting number of messages in total to find id of first message in
    //block and updating it
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));

    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open info file for reading in chat" << chatID;
        return;
    }

    QJsonObject jsonObj = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object();
    infoFile.close();

    size_t totalMessages = jsonObj["total_messages"].toInt(),
           firstMessageInBlockID = totalMessages - (totalMessages % Server::messagesBlockSize);

    jsonObj["total_messages"] = QJsonValue::fromVariant(totalMessages + 1);

    QJsonDocument jsonDoc(jsonObj);
    if (!infoFile.open(QIODevice::Truncate | QIODevice::WriteOnly))
    {
        qDebug() << "Unable to open info file for writing in chat" << chatID;
        return;
    }

    infoFile.write(jsonDoc.toJson());
    infoFile.close();

    //putting a message into a file
    QFile messagesFile(QStringLiteral("chats/%1/%2.json").arg(chatID).arg(firstMessageInBlockID));
    if (!messagesFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        if (!messagesFile.open(QIODevice::WriteOnly))
        {
            qDebug() << "1Unable to open messages file for writing in chat" << chatID;
            return;
        }
        messagesFile.write("{\"messages\": []}");
        messagesFile.close();
        if (!messagesFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open messages file for reading in chat" << chatID;
            return;
        }
    }
    QString formattedDateTime = QStringLiteral("%1 %2").arg(
                   QDate::currentDate().toString("dd.MM.yyyy")).arg(
                   QTime::currentTime().toString("hh:mm:ss"));

    jsonObj = QJsonDocument::fromJson(QString(messagesFile.readAll()).toUtf8()).object();
    messagesFile.close();

    QJsonArray messagesArr = jsonObj["messages"].toArray();
    QJsonObject jsonMessage;
    jsonMessage.insert("id",        QJsonValue::fromVariant(totalMessages));
    jsonMessage.insert("text",      QJsonValue::fromVariant(messageText));
    jsonMessage.insert("sender_id", QJsonValue::fromVariant(senderID));
    jsonMessage.insert("date",      QJsonValue::fromVariant(formattedDateTime));

    messagesArr.append(jsonMessage);
    jsonObj["messages"] = messagesArr;

    if (!messagesFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "2Unable to open messages file for writing in chat" << chatID;
        return;
    }

    jsonDoc = QJsonDocument(jsonObj);
    messagesFile.write(jsonDoc.toJson());
    messagesFile.close();

    //qDebug() << "Message successfully submitted";
}

bool Server::isMemberOfChat(const size_t &userID, const size_t &chatID)
{
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    QJsonArray jsonArr = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object()["members"].toArray();
    return jsonArr.contains(QJsonValue::fromVariant(userID));
}

size_t Server::getTotalMessages(const size_t &chatID, const size_t &querySenderID)
{
    if (!Server::isMemberOfChat(querySenderID, chatID))
        throw UserIsNotMemberOfChatException();

    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    QJsonObject jsonObj = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object();
    infoFile.close();
    return jsonObj["total_messages"].toInt();
}

QJsonObject Server::getMessageByID(const size_t &chatID, const size_t &messageID, const size_t &querySenderID)
{
    if (!Server::isMemberOfChat(querySenderID, chatID))
        throw UserIsNotMemberOfChatException();

    size_t firstMessageInBlockID = messageID - (messageID % Server::messagesBlockSize);
    QFile messageFile(QStringLiteral("chats/%1/%2.json").arg(chatID).arg(firstMessageInBlockID));
    QJsonArray jsonArr = QJsonDocument::fromJson(QString(messageFile.readAll()).toUtf8()).object()["messages"].toArray();
    messageFile.close();
    return jsonArr[messageID - firstMessageInBlockID].toObject();
}

QJsonArray Server::getLastBlockOfMessages(const size_t &chatID, const size_t &querySenderID)
{
    if (!Server::isMemberOfChat(querySenderID, chatID))
        throw UserIsNotMemberOfChatException();

    size_t totalMessages = getTotalMessages(chatID, querySenderID);
    size_t firstMessageInBlockID = (totalMessages - 1) - (totalMessages - 1) % Server::messagesBlockSize;
    QFile messageFile(QStringLiteral("chats/%1/%2.json").arg(chatID).arg(firstMessageInBlockID));
    QJsonArray jsonArr = QJsonDocument::fromJson(QString(messageFile.readAll()).toUtf8()).object()["messages"].toArray();
    messageFile.close();
    return jsonArr;
}

QJsonObject Server::getChatInfo(const size_t &chatID)
{
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    QJsonObject jsonObj = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object();
    if (!jsonObj["is_visible"].toBool())
        throw ChatIsNotVisibleException();
    return jsonObj;
}
