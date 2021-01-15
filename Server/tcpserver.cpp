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

QJsonObject Server::generateErrorJson(const apiErrorCode &err)
{
    QJsonObject error;
    error.insert("error_code", err);
    error.insert("error_desc", Server::apiErrorCodeDesc(err));
    return error;
}

QJsonObject Server::callApiMethod(const QString &method, const QJsonObject &params)
{
    //first of all checking if the user wants ot change access token
    //for other queries access token is essential
    if (method == "access_token.change")
    {
        apiErrorCode apiErr = NULL_ERROR;

        if (params["user_id"] == QJsonValue::Undefined)
            apiErr = NO_USER_ID;

        else if (params["password"] == QJsonValue::Undefined)
            apiErr = NO_USER_PASSWORD;

        else if (params["user_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        else if (!Server::validateUser(params["user_id"].toInt(),
                                       params["password"].toString()))
            apiErr = USER_VALIDATION_FAILURE;

        if (apiErr != apiErrorCode::NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        QString newToken = Server::updAccessToken(params["user_id"].toInt());
        QJsonObject response;
        response.insert("new_token", newToken);
        return response;
    }

    if (params["access_token"] == QJsonValue::Undefined)
        return Server::generateErrorJson(apiErrorCode::NO_ACCESS_TOKEN);

    if (params["access_token"].toString().length() != Server::accessTokenLen)
        return Server::generateErrorJson(apiErrorCode::INCORRECT_VALUE);

    size_t senderID;
    try
    {
        senderID = Server::getIDFromAccessToken(params["access_token"].toString());
    }
    catch (const TokenDoesNotExistException &e)
    {
        return Server::generateErrorJson(apiErrorCode::TOKEN_VALIDATION_FAILURE);
    }

    if (method == "chat.get")
    {
        apiErrorCode apiErr = NULL_ERROR;

        if (params["chat_id"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_ID;

        else if (params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        size_t chatID = params["chat_id"].toInt();

        QJsonObject response;
        try
        {
            response = Server::getChatInfo(chatID, senderID);
        }
        catch (const ChatIsNotVisibleException &e)
        {
            return Server::generateErrorJson(apiErrorCode::CHAT_IS_NOT_VISIBLE);
        }

        return response;
    }

    else if (method == "chat.set.property")
    {
        apiErrorCode apiErr = apiErrorCode::NULL_ERROR;
        QString property, value;
        QJsonObject chatInfo;
        size_t chatID;

        if (params["chat_id"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_ID;

        else if (params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        else if (params["property"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_PROPERTY;

        else if (params["value"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_PROPERTY_VALUE;
        else
        {
            property = params["property"].toString();
            value = params["value"].toString();
            chatID = params["chat_id"].toInt();
            QJsonObject chatInfo = Server::getChatInfo(chatID, senderID);
            if (chatInfo[property] == QJsonValue::Undefined ||
                chatInfo[property].toString() == "admin" ||
                chatInfo[property].toString() == "members" ||
                chatInfo[property].toString() == "total_messages")
                apiErr = INCORRECT_VALUE;
        }

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        chatInfo[property] = value;
        QJsonObject response;
        try
        {
            response = Server::setChatInfo(chatID, senderID, chatInfo);
        }
        catch (const UserIsNotAdminException &e)
        {
            return Server::generateErrorJson(apiErrorCode::USER_NOT_ADMIN);
        }

        return response;
    }

    else if (method == "chat.addmember")
    {
        apiErrorCode apiErr = apiErrorCode::NULL_ERROR;

        if (params["chat_id"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_ID;

        else if (params["user_id"] == QJsonValue::Undefined)
            apiErr = NO_USER_ID;

        else if (params["user_id"].toInt() < 0 ||
                 params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        try
        {
            QJsonObject response = Server::addMemberInChatByUser(
                        params["chat_id"].toInt(),
                        senderID,
                        params["user_id"].toInt());
            QJsonObject serverMessageResponse = Server::sendMessage(
                        params["chat_id"].toInt(),
                        QStringLiteral("%1 was invited in chat").arg(Server::getUsernameByID(params["user_id"].toInt())),
                        -1,
                        true);
            return response;
        }
        catch (const UserIsNotMemberOfChatException &e)
        {
            return Server::generateErrorJson(USER_NOT_IN_CHAT);
        }
        catch (const UserNotFoundException &e)
        {
            return Server::generateErrorJson(USER_DOES_NOT_EXIST);
        }
    }

    else if (method == "chat.kickmember")
    {
        apiErrorCode apiErr = apiErrorCode::NULL_ERROR;

        if (params["chat_id"] == QJsonValue::Undefined)
            apiErr = NO_CHAT_ID;

        else if (params["user_id"] == QJsonValue::Undefined)
            apiErr = NO_USER_ID;

        else if (params["user_id"].toInt() < 0 ||
                 params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        try
        {
            QJsonObject response = Server::kickMember(
                        params["chat_id"].toInt(),
                        senderID,
                        params["user_id"].toInt());
            QJsonObject serverMessageResponse = Server::sendMessage(
                        params["chat_id"].toInt(),
                        QStringLiteral("%1 was kicked from chat").arg(Server::getUsernameByID(params["user_id"].toInt())),
                        -1,
                        true);
            return response;
        }
        catch (const UserIsNotAdminException &e)
        {
            return Server::generateErrorJson(USER_NOT_ADMIN);
        }
    }

    return Server::generateErrorJson(apiErrorCode::UNKNOWN_ERROR);
}

QString Server::apiErrorCodeDesc(const apiErrorCode &err)
{
    switch (err)
    {
    case NULL_ERROR:
        return "";
        break;

    case NO_ACCESS_TOKEN:
        return "No access token provided to API";
        break;

    case NO_CHAT_ID:
        return "Query parameter not found: chat_id";
        break;

    case NO_QUERY_SENDER_ID:
        return "Unknown query sender id, unable to validate access token";
        break;

    case INCORRECT_VALUE:
        return "Some of the fields of query contain prohibited values";
        break;

    case TOKEN_VALIDATION_FAILURE:
        return "Incorrect token sent, please check it's correctness";
        break;

    default:
        return "Unknown error";
        break;
    }
}

bool Server::validateUser(const size_t &userID, const QString &userPassword)
{
    const QString pathToData = "dbase/userlogindata";
    const size_t fileID = userID / Server::userLoginDataBlockSize;

    if (fileID >= static_cast<size_t>(QDir(pathToData).count() - 2))
        return false;

    QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(fileID));
    if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open dbase/userlogindata for reading";
        return false;
    }
    while (!dataFile.atEnd())
    {
        QString line = dataFile.readLine();
        QStringList list = line.split(' ');
        if (list[0].toUInt() == userID && list[2] == userPassword + "\n")
            return true;
    }
    dataFile.close();
    return false;
}

void Server::createUser(const QString &username, const QString &password)
{
    auto countNumberOfLines = [](QFile &file) //though its reference function doesnt write anything to file
    {
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open file to count lines";
            return static_cast<size_t>(0);
        }
        size_t counter = 0;
        while (!file.atEnd())
        {
            file.readLine();
            ++counter;
        }
        file.close();
        return counter;
    };

    const QString pathToData = "dbase/userlogindata";
    QDir().mkdir("dbase");
    QDir().mkdir(pathToData);
    size_t totalFiles = QDir(pathToData).count() - 2;
    if (totalFiles == 0)
    {
        QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles));
        if (!dataFile.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open dbase/userlogindata for writing";
            return;
        }
        QTextStream out(&dataFile);
        out << QStringLiteral("0 %1 %2").arg(username).arg(password) << Qt::endl;
        dataFile.close();
        return;
    }
    QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles - 1));
    size_t lines = countNumberOfLines(dataFile);
    if (lines == Server::userLoginDataBlockSize)
    {
        dataFile.setFileName(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles++));
        lines = 0;
    }
    if (!dataFile.open(QIODevice::Append))
    {
        qDebug() << "Unable to open dataFile for appending";
        return;
    }
    size_t newUserID = (totalFiles - 1) * Server::userLoginDataBlockSize + lines;
    QTextStream out(&dataFile);
    out << QStringLiteral("%1 %2 %3").arg(newUserID).arg(username).arg(password) << Qt::endl;
    return;
}

size_t Server::getIDFromAccessToken(const QString &accessToken)
{
    QDir().mkdir("dbase");
    QFile in("dbase/access_tokens");
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        in.close();
        if (!in.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open access_tokens for writing";
            return false;
        }
        in.close();
        if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open access_tokens for reading";
            return false;
        }
    }
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList pairOfValues = line.split(' ');

        QString token = pairOfValues[1];
        if (accessToken + "\n" == token)
            return pairOfValues[0].toInt();
    }
    in.close();
    throw TokenDoesNotExistException();
}

QString Server::generateAccessToken()
{
    const QString alphabet = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm.-0123456789_=/|";
    QString ans;
    for (size_t i = 0; i < Server::accessTokenLen; ++i)
        ans += alphabet[QRandomGenerator::global()->generate() % alphabet.length()];
    return ans;
}

QString Server::updAccessToken(const size_t &senderID)
{
    QDir().mkdir("dbase");
    QFile in("dbase/access_tokens");
    if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        in.close();
        if (!in.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open access_tokens for writing";
            return "";
        }
        in.close();
        if (!in.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open access_tokens for reading";
            return "";
        }
    }

    QStringList temp;
    QString newAccessToken = Server::generateAccessToken();
    bool isUpdated = false;
    while (!in.atEnd())
    {
        QString line = in.readLine();
        QStringList pairOfValues = line.split(' ');
        size_t id = pairOfValues[0].toInt();
        if (id == senderID)
        {
            isUpdated = true;
            pairOfValues[1] = newAccessToken + "\n";
        }
        temp.append(pairOfValues[0] + " " + pairOfValues[1]);
    }

    if (!isUpdated)
        temp.append(QStringLiteral("%1 %2\n").arg(senderID).arg(newAccessToken));

    in.close();
    if (!in.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "Unable to open access_tokens for writing";
        return "";
    }
    QTextStream out(&in);
    for (QString i: temp)
        out << i;
    qDebug() << "User" << senderID << "token updated";
    return newAccessToken;
}

QString Server::getUsernameByID(const size_t &userID)
{
    const size_t fileID = userID / Server::userLoginDataBlockSize;
    QFile dataFile(QStringLiteral("dbase/userlogindata/%1").arg(fileID));
    if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open dbase/userlogindata for reading";
        throw UserNotFoundException();
    }
    while (!dataFile.atEnd())
    {
        QString line = dataFile.readLine();
        QStringList userDataValues = line.split(' ');
        if (static_cast<size_t>(userDataValues[0].toInt()) == userID)
            return userDataValues[1];
    }
    dataFile.close();
    throw UserNotFoundException();
}

QString Server::parseQuery(const QString &query)
{
    QJsonObject jsonObj = QJsonDocument::fromJson(query.toUtf8()).object();
    QString method = jsonObj["method"].toString();
    QJsonObject jsonParams = jsonObj["params"].toObject();

    QJsonObject response = Server::callApiMethod(method, jsonParams);
    return QJsonDocument(response).toJson();
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

QJsonObject Server::sendMessage(const size_t&           chatID,
                                const QString&          messageText,
                                const size_t&           senderID,
                                const bool&             isSystem)
{
    if (!isSystem && !Server::isMemberOfChat(senderID, chatID))
    {
        qDebug() << "Can't send message: user" << senderID << "is not member of chat" << chatID;
        return Server::generateErrorJson(USER_NOT_IN_CHAT);
    }
    //getting number of messages in total to find id of first message in
    //block and updating it
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));

    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open info file for reading in chat" << chatID;
        return Server::generateErrorJson(UNKNOWN_ERROR);
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
        return Server::generateErrorJson(UNKNOWN_ERROR);
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
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        messagesFile.write("{\"messages\": []}");
        messagesFile.close();
        if (!messagesFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open messages file for reading in chat" << chatID;
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
    }
    QString formattedDateTime = QStringLiteral("%1 %2").arg(
                   QDate::currentDate().toString("dd.MM.yyyy")).arg(
                   QTime::currentTime().toString("hh:mm:ss"));

    jsonObj = QJsonDocument::fromJson(QString(messagesFile.readAll()).toUtf8()).object();
    messagesFile.close();

    QJsonArray messagesArr = jsonObj["messages"].toArray();
    QJsonObject jsonMessage;
    if (isSystem)
    {
        jsonMessage.insert("id",   QJsonValue::fromVariant(totalMessages));
        jsonMessage.insert("type", QJsonValue::fromVariant("system"));
        jsonMessage.insert("text", QJsonValue::fromVariant(messageText));
        jsonMessage.insert("date", QJsonValue::fromVariant(formattedDateTime));
    }
    else
    {
        jsonMessage.insert("id",        QJsonValue::fromVariant(totalMessages));
        jsonMessage.insert("text",      QJsonValue::fromVariant(messageText));
        jsonMessage.insert("sender_id", QJsonValue::fromVariant(senderID));
        jsonMessage.insert("date",      QJsonValue::fromVariant(formattedDateTime));
    }

    messagesArr.append(jsonMessage);
    jsonObj["messages"] = messagesArr;

    if (!messagesFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "2Unable to open messages file for writing in chat" << chatID;
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    jsonDoc = QJsonDocument(jsonObj);
    messagesFile.write(jsonDoc.toJson());
    messagesFile.close();

    return Server::generateErrorJson(NULL_ERROR);
}

bool Server::isMemberOfChat(const size_t &userID, const size_t &chatID)
{
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for reading";
        return false;
    }
    QJsonArray jsonArr = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object()["members"].toArray();
    infoFile.close();
    return jsonArr.contains(QJsonValue::fromVariant(userID));
}

bool Server::isAdmin(const size_t &userID, const size_t &chatID)
{
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for reading";
        return false;
    }
    QJsonObject info = QJsonDocument::fromJson(infoFile.readAll()).object();
    infoFile.close();
    return (static_cast<size_t>(info["admin"].toInt()) == userID);
}

size_t Server::getTotalMessages(const size_t &chatID, const size_t &querySenderID)
{
    if (!Server::isMemberOfChat(querySenderID, chatID))
        throw UserIsNotMemberOfChatException();

    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open message file for reading";
        return 0;
    }
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
    if (!messageFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open message file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }
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
    if (!messageFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open message file for reading";
        return QJsonArray();
    }
    QJsonArray jsonArr = QJsonDocument::fromJson(QString(messageFile.readAll()).toUtf8()).object()["messages"].toArray();
    messageFile.close();
    return jsonArr;
}

QJsonObject Server::getChatInfo(const size_t &chatID, const size_t &senderID)
{
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }
    QJsonObject jsonObj = QJsonDocument::fromJson(QString(infoFile.readAll()).toUtf8()).object();
    QJsonArray members = jsonObj["members"].toArray();
    if (!jsonObj["is_visible"].toBool() && !members.contains(QJsonValue::fromVariant(senderID)))
        throw ChatIsNotVisibleException();
    return jsonObj;
}

QJsonObject Server::setChatInfo(const size_t &chatID, const size_t &senderID, const QJsonObject &chatInfo)
{
    if (!Server::isAdmin(senderID, chatID))
        throw UserIsNotAdminException();

    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));

    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "Unable to open chat" << chatID << "info file to write";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    infoFile.write(QJsonDocument(chatInfo).toJson());
    infoFile.close();
    return Server::generateErrorJson(NULL_ERROR);
}

QJsonObject Server::addMemberInChatByUser(const size_t &chatID, const size_t &senderID, const size_t &userToAddID)
{
    if (!Server::isMemberOfChat(senderID, chatID))
        throw UserIsNotMemberOfChatException();

    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    QJsonObject chatInfo = QJsonDocument::fromJson(infoFile.readAll()).object();
    infoFile.close();
    QJsonArray members = chatInfo["members"].toArray();

    if (members.contains(QJsonValue::fromVariant(userToAddID)))
        return Server::generateErrorJson(USER_ALREADY_IN_CHAT);

    members.append(QJsonValue::fromVariant(userToAddID));
    chatInfo["members"] = QJsonValue::fromVariant(members);

    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for writing";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    infoFile.write(QJsonDocument(chatInfo).toJson());
    infoFile.close();
    return Server::generateErrorJson(NULL_ERROR);
}

QJsonObject Server::kickMember(const size_t &chatID, const size_t &senderID, const size_t &userToKickID)
{
    if (!Server::isAdmin(senderID, chatID))
        throw UserIsNotAdminException();
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    QJsonObject chatInfo = QJsonDocument::fromJson(infoFile.readAll()).object();
    infoFile.close();
    QJsonArray members = chatInfo["members"].toArray();

    if (!members.contains(QJsonValue::fromVariant(userToKickID)))
        return Server::generateErrorJson(USER_NOT_IN_CHAT);

    for (int i = 0; i < members.size(); ++i)
        if (static_cast<size_t>(members[i].toInt()) == userToKickID)
        {
            members.removeAt(i);
            break;
        }

    chatInfo["members"] = QJsonValue::fromVariant(members);

    if (!infoFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "Unable to open chat" << chatID << "info file for writing";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }

    infoFile.write(QJsonDocument(chatInfo).toJson());
    infoFile.close();
    return Server::generateErrorJson(NULL_ERROR);
}
