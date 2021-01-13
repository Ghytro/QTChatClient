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

public slots:
    void slotNewConnection();
    void slotReadClient();

private:
    static const unsigned messagesBlockSize = 200;

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

    static QJsonObject getChatInfo(const size_t &chatID);

};

#endif // TCPSERVER_H
