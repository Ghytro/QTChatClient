#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QtCore>
#include <QTcpServer>
#include <QTcpSocket>

class Server : public QObject
{
    Q_OBJECT
public:
    explicit Server(quint16 port);
    virtual ~Server();

    static void debugCreateUser(const QString &username,
                                const QString &password)
    {
        Server::createUser(username, password);
    }

    static void debugAddChatMembership(const size_t &userID,
                                       const size_t &chatID)
    {
        Server::addChatMembership(userID, chatID);
    }

    static void debugDeleteChatMembership(const size_t &userID,
                                       const size_t &chatID)
    {
        Server::deleteChatMembership(userID, chatID);
    }

    static void debugValidateUser(const size_t      &userID,
                                  const QString&    userPassword)
    {
        qDebug() << Server::validateUser(userID, userPassword);
    }

    static void debugCreateChat(const QString&   chatName,
                         const QJsonArray&       membersIDs,
                         const size_t&           adminID,
                         const bool&             isVisible)
    {
        Server::createChat(chatName, membersIDs, adminID, isVisible);
    }

    static void debugSetChatInfo(const size_t&      chatID,
                                 const size_t&      senderID,
                                 const QJsonObject& chatInfo)
    {
        qDebug() << Server::setChatInfo(chatID, senderID, chatInfo);
    }

    static void debugAddMemberInChatByUser(const size_t &chatID,
                                           const size_t &senderID,
                                           const size_t &userToAddID)
    {
        qDebug() << Server::addMemberInChatByUser(chatID, senderID, userToAddID);
    }

    static void debugKickMember(const size_t &chatID,
                                const size_t &senderID,
                                const size_t &userToKickID)
    {
        qDebug() << Server::kickMember(chatID, senderID, userToKickID);
    }

    static void debugSendMessage(const size_t&   chatID,
                          const QString&         message,
                          const size_t&          senderID)
    {
        qDebug() << Server::sendMessage(chatID, message, senderID);
    }

    static void debugSendSystemMessage(const size_t&  chatID,
                                       const QString& text)
    {
        qDebug() << Server::sendMessage(chatID, text, -1, true);
    }

    static void debugGetMessageByID(const size_t& chatID,
                                    const size_t& messageID,
                                    const size_t& senderID)
    {
        qDebug() << Server::getMessageByID(chatID, messageID, senderID);
    }

    static void debugGetLastBlockOfMessages(const size_t& chatID,
                                            const size_t& senderID)
    {
        qDebug() << Server::getLastBlockOfMessages(chatID, senderID);
    }

    static void debugUpdAccessToken(const size_t& senderID)
    {
        qDebug() << Server::updAccessToken(senderID);
    }

    static void debugGetIDFromAccessToken(const QString &accessToken)
    {
        qDebug() << Server::getIDFromAccessToken(accessToken);
    }

public slots:
    void slotNewConnection();
    void slotReadClient();

private:
    QTcpServer *server;
    static QMap<QString, size_t> tokens;
    static QMap<QString, size_t> usernames;
    static void loadTokensMap();
    static void loadUsernamesMap();

    static const unsigned messagesBlockSize = 200;
    static const unsigned userLoginDataBlockSize = 200;
    static const unsigned accessTokenDataBlockSize = 200;
    static const unsigned userChatMembershipBlockSize = 200;
    static const unsigned accessTokenLen = 100;

    enum apiErrorCode
    {
        NULL_ERROR, // no errors
        NO_ACCESS_TOKEN,
        NO_CHAT_ID,
        NO_QUERY_SENDER_ID,
        INCORRECT_VALUE,
        TOKEN_VALIDATION_FAILURE,
        NO_USER_ID,
        NO_USERNAME,
        NO_USER_PASSWORD,
        USER_VALIDATION_FAILURE,
        USER_DOES_NOT_EXIST,
        USER_ALREADY_EXISTS,
        CHAT_DOESNT_EXIST,
        CHAT_IS_NOT_VISIBLE,
        USER_NOT_ADMIN,
        USER_NOT_IN_CHAT,
        NO_CHAT_PROPERTY,
        NO_CHAT_PROPERTY_VALUE,
        USER_ALREADY_IN_CHAT,
        NO_MESSAGE_TEXT,
        NO_CHAT_VISIBILITY,
        NO_CHAT_NAME,
        NO_CHAT_MEMBERS,
        UNKNOWN_ERROR
    };

    static QJsonObject createUser(const QString &username,
                           const QString &password);

    static QString apiErrorCodeDesc(const apiErrorCode&);
    static QJsonObject generateErrorJson(const apiErrorCode&);

    static bool validateUser(const size_t&  userID,
                             const QString& userPassword);
    static size_t getIDFromAccessToken(const QString&);
    static QJsonObject updAccessToken(const size_t& senderID);
    static QString generateAccessToken();

    static QString getUsernameByID(const size_t&);
    static size_t getIDFromUsername(const QString&);

    static QString parseQuery(const QString&);

    static QJsonObject createChat(const QString&   chatName,
                    const QJsonArray&       membersIDs,
                    const size_t&           adminID,
                    const bool&             isVisible);

    static QJsonObject sendMessage(const size_t&   chatID,
                                   const QString&  message,
                                   const size_t&   senderID,
                                   const bool&     isSystem=false);

    static bool isMemberOfChat(const size_t &userID,
                               const size_t &chatID);

    static bool isAdmin(const size_t &userID,
                        const size_t &chatID);

    static size_t getTotalMessages(const size_t &chatID,
                                   const size_t &querySenderID);

    static QJsonObject getMessageByID(const size_t &chatID,
                                      const size_t &messageID,
                                      const size_t &querySenderID);

    static QJsonArray getLastBlockOfMessages(const size_t &chatID,
                                             const size_t &querySenderID);

    static QJsonObject getChatInfo(const size_t &chatID,
                                   const size_t &senderID);

    static QJsonObject setChatInfo(const size_t&       chatID,
                                   const size_t&       senderID,
                                   const QJsonObject&  chatInfo);

    static QJsonObject addMemberInChatByUser(const size_t& chatID,
                                             const size_t& senderID,
                                             const size_t& userToAddID);

    static QJsonObject kickMember(const size_t& chatID,
                                  const size_t& senderID,
                                  const size_t& userToKickID);

    static void addChatMembership(const size_t &userID,
                                  const size_t &chatID);

    static void deleteChatMembership(const size_t &userID,
                                     const size_t &chatID);

    static QJsonArray getChatMembership(const size_t &userID);

    static QJsonObject callApiMethod(const QString&     method,
                                     const QJsonObject& params);

};

#endif // TCPSERVER_H
