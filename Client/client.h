#ifndef CLIENT_H
#define CLIENT_H

#include <QLabel>
#include <QMainWindow>
#include <QtCore>
#include <QTcpSocket>
#include <QTcpServer>

class Client: public QObject
{
    Q_OBJECT

public:
    Client(const quint16 &hostPort);
    virtual ~Client();

public slots:
    void sendData(const QByteArray &data);

private slots:
    void slotReadyRead();
    void slotConnected();
    void slotError(QAbstractSocket::SocketError);

signals:
    void setErrorLabelText(QString);
    void switchToChatWindow();
    void chatSuccessfullyCreated();

private:
    QTcpSocket *socket;
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

#endif // CLIENT_H
