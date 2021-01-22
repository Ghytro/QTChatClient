#include "client.h"

Client::Client(const quint16 &hostPort)
{
    this->socket = new QTcpSocket;
    this->hostPort = hostPort;

    connect(this->socket, SIGNAL(readyRead()),
            this,         SLOT(slotReadyRead()));

    connect(this->socket, SIGNAL(connected()),
            this,         SLOT(slotConnected()));

    connect(this->socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this,         SLOT(slotError(QAbstractSocket::SocketError)));
}

Client::~Client()
{
    this->socket->deleteLater();
}

void Client::slotConnected()
{
    qDebug() << "Connection with server established!";
}

void Client::slotError(QAbstractSocket::SocketError err)
{
    QString error = "An error occured: ";

    if (err == QAbstractSocket::HostNotFoundError)
        error += "host not found.";
    else if (err == QAbstractSocket::RemoteHostClosedError)
        error += "remote host is closed.";
    else if (err == QAbstractSocket::ConnectionRefusedError)
        error += "connection refused.";

    emit setErrorLabelText(error);
    this->socket->close();
}

void Client::slotReadyRead()
{
    QByteArray data = this->socket->readAll();
    this->socket->close();
    QJsonObject jsonObj = QJsonDocument::fromJson(data).object();
    if (jsonObj["error_code"] == QJsonValue::Null)
    {
        if (jsonObj["new_token"] != QJsonValue::Null) //access_token.update or user.create was called
        {
            QFile tokenFile("token");
            if (!tokenFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
            {

                return;
            }
            tokenFile.write(jsonObj["new_token"].toString().toUtf8());
            tokenFile.close();
            emit switchToChatWindow();
        }
    }
    else
    {
        switch (jsonObj["error_code"].toInt())
        {
        case USER_ALREADY_EXISTS:
            emit setErrorLabelText("User with this username already exists");
            break;

        case USER_VALIDATION_FAILURE:
            emit setErrorLabelText("Incorrect username or password");
            break;

        default:
            break;
        }
    }
}

void Client::sendData(const QByteArray &data)
{
    this->socket->connectToHost(QHostAddress::LocalHost, this->hostPort);
    this->socket->write(data);
}
