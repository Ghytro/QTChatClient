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

    static void debugCreateChat(const QString&          chatName,
                         const QJsonArray&       membersIDs,
                         const size_t&           adminID,
                         const bool&             isVisible)
    {
        Server::createChat(chatName, membersIDs, adminID, isVisible);
    }

    static void debugSendMessage(const size_t&          chatID,
                          const QString&         message,
                          const size_t&          senderID)
    {
        Server::sendMessage(chatID, message, senderID);
    }

    static void debugUpdAccessToken(const size_t& senderID)
    {
        Server::updAccessToken(senderID);
    }

public slots:
    void slotNewConnection();
    void slotReadClient();

private:
    static const unsigned messagesBlockSize = 200;
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
        NO_USER_PASSWORD,
        USER_VALIDATION_FAILURE,
        CHAT_DOESNT_EXIST,
        CHAT_IS_NOT_VISIBLE,
        USER_NOT_ADMIN,
        UNKNOWN_ERROR
    };

    static QString apiErrorCodeDesc(const apiErrorCode&);
    static QJsonObject generateErrorJson(const apiErrorCode&);

    static bool validateUser(const size_t&  userID,
                             const QString& userPassword);
    static size_t getIDFromAccessToken(const QString&);
    static QString updAccessToken(const size_t& senderID);
    static QString generateAccessToken();

    QTcpServer *server;

    static QString parseQuery(const QString&);

    static void createChat(const QString&          chatName,
                    const QJsonArray&       membersIDs,
                    const size_t&           adminID,
                    const bool&             isVisible);

    static void sendMessage(const size_t&          chatID,
                     const QString&         message,
                     const size_t&          senderID);

    static bool isMemberOfChat(const size_t &userID,
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

    static QJsonObject callApiMethod(const QString&     method,
                                     const QJsonObject& params);

};

#endif // TCPSERVER_H
