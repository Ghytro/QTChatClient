#include "tcpserver.h"
#include "exceptions.h"

QMap<QString, size_t> Server::tokens = QMap<QString, size_t>();
QMap<QString, size_t> Server::usernames = QMap<QString, size_t>();

Server::Server(quint16 port)
{
    this->server = new QTcpServer;
    if (!this->server->listen(QHostAddress::LocalHost, port))
    {
        qDebug() << "Unable to listen port" << port;
        return;
    }

    connect(this->server, SIGNAL(newConnection()), this, SLOT(slotNewConnection()));

    Server::loadTokensMap();
    Server::loadUsernamesMap();

    //qDebug() << Server::usernames;

    qDebug() << "Server started";
}

Server::~Server()
{
    this->server->deleteLater();
}

void Server::loadTokensMap()
{
    if (!QDir("dbase/access_tokens").exists())
        return;
    size_t sz = QDir("dbase/access_tokens").count() - 2;
    for (size_t i = 0; i < sz; ++i)
    {
        QFile file(QStringLiteral("dbase/access_tokens/%1").arg(i));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open tokens file for reading";
            return;
        }
        while (!file.atEnd())
        {
            QString line = file.readLine();
            QStringList pairOfValues = line.split(' ');
            Server::tokens.insert(pairOfValues[1].left(pairOfValues[1].length() - 1), pairOfValues[0].toUInt());
        }
        file.close();
    }
}

void Server::loadUsernamesMap()
{
    if (!QDir("dbase/userlogindata").exists())
        return;
    size_t sz = QDir("dbase/userlogindata").count() - 2;
    for (size_t i = 0; i < sz; ++i)
    {
        QFile file(QStringLiteral("dbase/userlogindata/%1").arg(i));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open tokens file for reading";
            return;
        }
        while (!file.atEnd())
        {
            QString line = file.readLine();
            QStringList values = line.split(' ');
            Server::usernames.insert(values[1], values[0].toUInt());
        }
        file.close();
    }
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
    QByteArray data = clientSocket->readAll();
    QString reply = Server::parseQuery(data);
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
    //first of all methods that dont need access tokens
    //for other queries access token is essential
    if (method == "access_token.change")
    {
        size_t userID;
        try
        {
            userID = Server::getIDFromUsername(params["username"].toString());
        }
        catch (const UserNotFoundException &e)
        {
            return Server::generateErrorJson(USER_VALIDATION_FAILURE);
        }

        //qDebug() << userID;
        apiErrorCode apiErr = NULL_ERROR;

        if (params["username"] == QJsonValue::Null)
            apiErr = NO_USER_ID;

        else if (params["password"] == QJsonValue::Null)
            apiErr = NO_USER_PASSWORD;

        else if (!Server::validateUser(userID,
                                       params["password"].toString()))
            apiErr = USER_VALIDATION_FAILURE;

        if (apiErr != apiErrorCode::NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        QJsonObject response = Server::updAccessToken(userID);
        return response;
    }
    else if (method == "user.create")
    {
        apiErrorCode apiErr = NULL_ERROR;
        if (params["username"] == QJsonValue::Null)
            apiErr = NO_USERNAME;
        else if (params["password"] == QJsonValue::Null)
            apiErr = NO_USER_PASSWORD;
        else
        {
            try
            {
                Server::getIDFromUsername(params["username"].toString());
                apiErr = USER_ALREADY_EXISTS;
            }
            catch (const UserNotFoundException &e)
            {
                //qDebug() << "User doesnt exist";
            }
        }

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        return Server::createUser(params["username"].toString(),
                                  params["password"].toString());
    }

    if (params["access_token"] == QJsonValue::Null)
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

    if (method == "user.getmyinfo")
    {
        QJsonObject response;
        response.insert("username", QJsonValue::fromVariant(Server::getUsernameByID(senderID)));
        response.insert("chat_membership", QJsonValue::fromVariant(Server::getChatMembership(senderID)));
        return response;
    }
    else if (method == "chat.get")
    {
        apiErrorCode apiErr = NULL_ERROR;

        if (params["chat_id"] == QJsonValue::Null)
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

        if (params["chat_id"] == QJsonValue::Null)
            apiErr = NO_CHAT_ID;

        else if (params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;

        else if (params["property"] == QJsonValue::Null)
            apiErr = NO_CHAT_PROPERTY;

        else if (params["value"] == QJsonValue::Null)
            apiErr = NO_CHAT_PROPERTY_VALUE;
        else
        {
            property = params["property"].toString();
            value = params["value"].toString();
            chatID = params["chat_id"].toInt();
            chatInfo = Server::getChatInfo(chatID, senderID);
            if (chatInfo[property] == QJsonValue::Null ||
                property == "admin" ||
                property == "members" ||
                property == "total_messages")
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

        if (params["chat_id"] == QJsonValue::Null)
            apiErr = NO_CHAT_ID;

        else if (params["user_id"] == QJsonValue::Null)
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

        if (params["chat_id"] == QJsonValue::Null)
            apiErr = NO_CHAT_ID;

        else if (params["user_id"] == QJsonValue::Null)
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

    else if (method == "chat.sendmessage")
    {
        apiErrorCode apiErr = apiErrorCode::NULL_ERROR;
        if (params["chat_id"] == QJsonValue::Null)
            apiErr = NO_CHAT_ID;
        else if (params["chat_id"].toInt() < 0)
            apiErr = INCORRECT_VALUE;
        else if (params["text"] == QJsonValue::Null)
            apiErr = NO_MESSAGE_TEXT;

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        return Server::sendMessage(params["chat_id"].toInt(),
                params["text"].toString(),
                senderID);
    }

    else if (method == "chat.getlastmessages")
    {
        apiErrorCode apiErr = NULL_ERROR;

        if (params["chat_id"] == QJsonValue::Null)
            apiErr = NO_CHAT_ID;
        else if (params["num"] == QJsonValue::Null)
            apiErr = NO_LAST_MESSAGES_NUM;

        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        try
        {
            QJsonObject response;

            response.insert("chat_id",
                            params["chat_id"].toInt());

            response.insert("newest_messages",
                            Server::getNewestMessages(
                                params["chat_id"].toInt(),
                                senderID,
                                params["num"].toInt())
                           );
            return response;
        }
        catch (const UserIsNotMemberOfChatException &e)
        {
            return Server::generateErrorJson(USER_NOT_IN_CHAT);
        }
    }

    else if (method == "chat.create")
    {
        apiErrorCode apiErr = apiErrorCode::NULL_ERROR;
        if (params["is_visible"] == QJsonValue::Null)
            apiErr = NO_CHAT_VISIBILITY;
        else if (params["name"] == QJsonValue::Null)
            apiErr = NO_CHAT_NAME;
        if (apiErr != NULL_ERROR)
            return Server::generateErrorJson(apiErr);

        QJsonArray members;
        if (params["members"] != QJsonValue::Null)
            members = params["members"].toArray();

        return Server::createChat(params["name"].toString(),
                                  members,
                                  senderID,
                                  params["is_visible"].toBool());
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

    case CHAT_IS_NOT_VISIBLE:
        return "Chat is not public, you need to be it's member to get info about it";
        break;

    case CHAT_DOESNT_EXIST:
        return "Chat with this id doesnt exist, check the correctness of query parameters";
        break;

    case USER_DOES_NOT_EXIST:
        return "User with this id doesn't exist, check the correctness of query parameters";
        break;

    case USER_NOT_ADMIN:
        return "You need to be admin of the chat to have access to this operations";
        break;

    case USER_ALREADY_IN_CHAT:
        return "User with this id is already in chat";
        break;

    case USER_NOT_IN_CHAT:
        return "User with this id is not a member of this chat";
        break;

    case USER_VALIDATION_FAILURE:
        return "Incorrect login or password";
        break;

    case NO_USER_ID:
        return "No user ID";
        break;

   case NO_USER_PASSWORD:
        return "No password";
        break;

   case NO_CHAT_PROPERTY:
        return "No chat property";
        break;

   case NO_CHAT_PROPERTY_VALUE:
        return "No new value of a property";
        break;

   case USER_ALREADY_EXISTS:
        return "User already exists";
        break;

   case UNKNOWN_ERROR:
        return "Unknown error";
        break;

    default:
        return "No error description";
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

QJsonObject Server::createUser(const QString &username, const QString &password)
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
    QString newUserAccessToken;

    //filling userlogindata
    QString pathToData = "dbase/userlogindata";
    QDir().mkdir("dbase");
    QDir().mkdir(pathToData);
    size_t totalFiles = QDir(pathToData).count() - 2;
    if (totalFiles == 0)
    {
        QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles));
        if (!dataFile.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open dbase/userlogindata for writing";
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        Server::usernames.insert(username, 0);
        newUserAccessToken = Server::updAccessToken(0)["new_token"].toString();
        QTextStream out(&dataFile);
        out << QStringLiteral("0 %1 %2").arg(username).arg(password) << Qt::endl;
        dataFile.close();
    }
    else
    {
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
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        size_t newUserID = (totalFiles - 1) * Server::userLoginDataBlockSize + lines;
        newUserAccessToken = Server::updAccessToken(newUserID)["new_token"].toString();
        Server::usernames.insert(username, newUserID);
        QTextStream out(&dataFile);
        out << QStringLiteral("%1 %2 %3").arg(newUserID).arg(username).arg(password) << Qt::endl;
        dataFile.close();
    }

    //filling chat membership
    pathToData = "dbase/userchatmembership";
    QDir().mkdir("dbase");
    QDir().mkdir(pathToData);

    totalFiles = QDir(pathToData).count() - 2;
    if (totalFiles == 0)
    {
        QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles));
        if (!dataFile.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open dbase/userchatmembership for writing";
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        QJsonObject userMembership;
        userMembership.insert("id", QJsonValue::fromVariant(0));
        userMembership.insert("chats", QJsonValue::fromVariant(QJsonArray()));
        QJsonArray arr;
        arr.append(userMembership);
        QJsonObject jsonObj;
        jsonObj.insert("membership", QJsonValue::fromVariant(arr));
        dataFile.write(QJsonDocument(jsonObj).toJson());
        dataFile.close();
    }
    else
    {
        QFile dataFile(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles - 1));
        if (!dataFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open userchatmembership file for reading";
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        QJsonObject jsonObj = QJsonDocument::fromJson(dataFile.readAll()).object();
        dataFile.close();
        QJsonArray arr = jsonObj["membership"].toArray();
        size_t lines = arr.size();

        if (lines == Server::userChatMembershipBlockSize)
        {
            dataFile.setFileName(QStringLiteral("%1/%2").arg(pathToData).arg(totalFiles));
            if (!dataFile.open(QIODevice::WriteOnly))
            {
                qDebug() << "Unable to open dbase/userchatmembership for writing";
                return Server::generateErrorJson(UNKNOWN_ERROR);
            }
            size_t newUserID = (totalFiles - 1) * Server::userLoginDataBlockSize + lines;
            QJsonObject userMembership;
            userMembership.insert("id", QJsonValue::fromVariant(newUserID));
            userMembership.insert("chats", QJsonValue::fromVariant(QJsonArray()));
            QJsonArray arr;
            arr.append(userMembership);
            QJsonObject jsonObj;
            jsonObj.insert("membership", QJsonValue::fromVariant(arr));
            dataFile.write(QJsonDocument(jsonObj).toJson());
            dataFile.close();
        }
        else
        {
            size_t newUserID = (totalFiles - 1) * Server::userLoginDataBlockSize + lines;
            QJsonObject userMembership;
            userMembership.insert("id", QJsonValue::fromVariant(newUserID));
            userMembership.insert("chats", QJsonValue::fromVariant(QJsonArray()));
            arr.append(userMembership);
            jsonObj["membership"] = arr;
            if (!dataFile.open(QIODevice::WriteOnly))
            {
                qDebug() << "Unable to open dbase/userchatmembership for writing";
                return Server::generateErrorJson(UNKNOWN_ERROR);
            }
            dataFile.write(QJsonDocument(jsonObj).toJson());
            dataFile.close();
        }
    }

    //in response we store only access token
    QJsonObject response;
    response.insert("new_token", QJsonValue::fromVariant(newUserAccessToken));
    return response;
}

size_t Server::getIDFromAccessToken(const QString &accessToken)
{
    if (Server::tokens.find(accessToken) == Server::tokens.end())
        throw UserNotFoundException();

    return Server::tokens[accessToken];
}

size_t Server::getIDFromUsername(const QString &username)
{
    if (Server::usernames.find(username) == Server::usernames.end())
        throw UserNotFoundException();

    return Server::usernames[username];
}

QString Server::generateAccessToken()
{
    const QString alphabet = "QWERTYUIOPASDFGHJKLZXCVBNMqwertyuiopasdfghjklzxcvbnm.-0123456789_=/|";
    QString ans;
    for (size_t i = 0; i < Server::accessTokenLen; ++i)
        ans += alphabet[QRandomGenerator::global()->generate() % alphabet.length()];
    return ans;
}

QJsonObject Server::updAccessToken(const size_t &senderID)
{
    QDir().mkdir("dbase");
    QDir().mkdir("dbase/access_tokens");

    size_t numOfFiles = QDir("dbase/access_tokens").count() - 2;
    QString newAccessToken = Server::generateAccessToken();

    size_t fileID = senderID / Server::accessTokenDataBlockSize;
    QFile tokenFile(QStringLiteral("dbase/access_tokens/%1").arg(fileID));

    if (fileID == numOfFiles) //condition to create new block
    {
        if (!tokenFile.open(QIODevice::WriteOnly))
        {
            qDebug() << "Unable to open token file for writing";
            return Server::generateErrorJson(UNKNOWN_ERROR);
        }
        tokenFile.write(QStringLiteral("%1 %2\n").arg(senderID).arg(newAccessToken).toUtf8());
        Server::tokens[newAccessToken] = senderID;
        QJsonObject response;
        response.insert("new_token", newAccessToken);
        tokenFile.close();
        return response;
    }

    QStringList updatedTokens;
    if (!tokenFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open token file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }
    bool updated = false;
    while (!tokenFile.atEnd())
    {
        QString line = tokenFile.readLine();
        QStringList pairOfValues = line.split(' ');
        if (pairOfValues[0].toUInt() == senderID)
        {
            updated = true;
            Server::tokens.remove(pairOfValues[1].left(pairOfValues[1].length() - 1));
            Server::tokens.insert(newAccessToken, senderID);
            pairOfValues[1] = newAccessToken + "\n";
        }
        updatedTokens.append(pairOfValues[0] + " " + pairOfValues[1]);
    }
    tokenFile.close();

    if (!updated)
        updatedTokens.append(QStringLiteral("%1 %2\n").arg(senderID).arg(newAccessToken));

    if (!tokenFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        qDebug() << "Unable to open token file for writing";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }
    QTextStream out(&tokenFile);
    for (QString i: updatedTokens)
        out << i;
    QJsonObject response;
    response.insert("new_token", newAccessToken);
    tokenFile.close();
    //qDebug() << response;
    return response;
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

QJsonObject Server::createChat(const QString&             chatName,
                           const QJsonArray&       membersIDs,
                           const size_t&           adminID,
                           const bool&             isVisible)
{
    QDir().mkdir("chats");
    size_t chatID = QDir("chats").count() - 2;
    QDir("chats").mkdir(QString::number(chatID));

    QJsonObject json;
    json.insert("name",             QJsonValue::fromVariant(chatName));
    json.insert("members",          QJsonValue::fromVariant(membersIDs));
    json.insert("admin",            QJsonValue::fromVariant(adminID));
    json.insert("is_visible",       QJsonValue::fromVariant(isVisible));
    json.insert("total_messages",   QJsonValue::fromVariant(0));

    QJsonDocument jsonDoc(json);
    QFile infoFile(QStringLiteral("chats/%1/info.json").arg(chatID));
    if (!infoFile.open(QFile::WriteOnly))
    {
        qDebug() << "Unable to open chat info file for reading";
        return Server::generateErrorJson(UNKNOWN_ERROR);
    }
    infoFile.write(jsonDoc.toJson());
    QJsonObject response;
    response.insert("chat_id", QJsonValue::fromVariant(chatID));
    return response;
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
           fileID = totalMessages / Server::messagesBlockSize;

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
    QFile messagesFile(QStringLiteral("chats/%1/%2.json").arg(chatID).arg(fileID));
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
        qDebug() << "Unable to open info file for reading";
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

void Server::addChatMembership(const size_t &userID, const size_t &chatID)
{
    size_t fileID = userID / Server::userChatMembershipBlockSize;
    QFile membershipFile(QStringLiteral("dbase/userchatmembership/%1").arg(fileID));
    if (!membershipFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open membership file for reading";
        return;
    }

    QJsonObject jsonObj = QJsonDocument::fromJson(membershipFile.readAll()).object();
    membershipFile.close();
    QJsonArray memberships = jsonObj["membership"].toArray();
    QJsonObject userData = memberships[userID % Server::userChatMembershipBlockSize].toObject();
    QJsonArray chats = userData["chats"].toArray();

    if (chats.contains(QJsonValue::fromVariant(chatID)))
        throw UserIsAlreadyInChatException();

    chats.append(QJsonValue::fromVariant(chatID));
    userData["chats"] = chats;
    memberships[userID % Server::userChatMembershipBlockSize] = userData;
    jsonObj["membership"] = memberships;

    if (!membershipFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "Unable to open membership file for writing";
        return;
    }

    membershipFile.write(QJsonDocument(jsonObj).toJson());
    membershipFile.close();
}

void Server::deleteChatMembership(const size_t &userID, const size_t &chatID)
{
    size_t fileID = userID / Server::userChatMembershipBlockSize;
    QFile membershipFile(QStringLiteral("dbase/userchatmembership/%1").arg(fileID));
    if (!membershipFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open membership file for reading";
        return;
    }

    QJsonObject jsonObj = QJsonDocument::fromJson(membershipFile.readAll()).object();
    membershipFile.close();
    QJsonArray memberships = jsonObj["membership"].toArray();
    QJsonObject userData = memberships[userID % Server::userChatMembershipBlockSize].toObject();
    QJsonArray chats = userData["chats"].toArray();

    if (!chats.contains(QJsonValue::fromVariant(chatID)))
        throw UserIsNotMemberOfChatException();

    for (int i = 0; i < chats.size(); ++i)
        if (chats[i] == QJsonValue::fromVariant(chatID))
        {
            chats.removeAt(i);
            break;
        }

    userData["chats"] = chats;
    memberships[userID % Server::userChatMembershipBlockSize] = userData;
    jsonObj["membership"] = memberships;

    if (!membershipFile.open(QIODevice::WriteOnly))
    {
        qDebug() << "Unable to open membership file for writing";
        return;
    }

    membershipFile.write(QJsonDocument(jsonObj).toJson());
    membershipFile.close();
}

QJsonArray Server::getChatMembership(const size_t &userID)
{
    size_t fileID = userID / Server::userChatMembershipBlockSize;
    QFile membershipFile(QStringLiteral("dbase/userchatmembership/%1").arg(fileID));
    if (!membershipFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open membership file for reading";
        return {};
    }

    QJsonObject jsonObj = QJsonDocument::fromJson(membershipFile.readAll()).object();
    membershipFile.close();
    QJsonArray memberships = jsonObj["membership"].toArray();
    QJsonObject userData = memberships[userID % Server::userChatMembershipBlockSize].toObject();
    QJsonArray chats = userData["chats"].toArray();
    QJsonArray response;
    for (QJsonValue i: chats)
    {
        QJsonValue chatName = Server::getChatInfo(i.toInt(), userID)["name"];

        QJsonObject obj;
        obj.insert("id", i);
        obj.insert("name", chatName);
        response.append(obj);
    }
    return response;
}

QJsonArray Server::getNewestMessages(const size_t &chatID,
                                     const size_t &querySenderID,
                                     int          messagesNum)
{
    if (messagesNum <= 0)
        return {};

    if (!Server::isMemberOfChat(querySenderID, chatID))
        throw UserIsNotMemberOfChatException();

    int fileNum = QDir(QStringLiteral("chats/%1").arg(chatID)).count() - 4;
    QJsonArray response;
    for (; messagesNum > 0 && fileNum >= 0; --fileNum)
    {
        QFile messageFile(QStringLiteral("chats/%1/%2.json").arg(chatID).arg(fileNum));
        if (!messageFile.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            qDebug() << "Unable to open message file for reading";
            return {};
        }
        QJsonArray messages = QJsonDocument::fromJson(messageFile.readAll()).object()["messages"].toArray();
        messageFile.close();
        for (int i = messages.size() - 1; messagesNum > 0 && i >= 0; --messagesNum, --i)
            response.prepend(messages[i]);
    }
    return response;
}
