#ifndef ASYNCCLIENT_H
#define ASYNCCLIENT_H

#include <QtCore>
#include <QTcpSocket>
#include <QTcpServer>

class AsyncClientManager;

class AsyncClient: public QObject
{
    Q_OBJECT
public:
    explicit AsyncClient(const quint16 &hostPort);
    void setManager(AsyncClientManager*);
    virtual ~AsyncClient();

public slots:
    void sendData(QByteArray);

private slots:
    void slotConnected();
    void slotReadyRead();
    void slotError(QAbstractSocket::SocketError);

signals:
    void updUsername(QString);
    void updChatList(QJsonArray);
    void updNewestMessages(size_t chatID,
                           QJsonArray messages);
    void gotReply();

private:
    QTcpSocket *socket;
    AsyncClientManager *manager;
    quint16 hostPort;
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
        NO_LAST_MESSAGES_NUM,
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
};

#endif // ASYNCCLIENT_H
