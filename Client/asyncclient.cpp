#include "asyncclient.h"

AsyncClient::AsyncClient(const quint16 &hostPort)
{
    this->socket = new QTcpSocket();
    this->hostPort = hostPort;

    connect(this->socket, SIGNAL(readyRead()),
            this,         SLOT(slotReadyRead()));

    connect(this->socket, SIGNAL(error(QAbstractSocket::SocketError)),
            this,         SLOT(slotError(QAbstractSocket::SocketError)));

    connect(this->socket, SIGNAL(connected()),
            this,         SLOT(slotConnected()));

    this->socket->connectToHost(QHostAddress::LocalHost, this->hostPort);
}

AsyncClient::~AsyncClient()
{
    this->socket->deleteLater();
}

void AsyncClient::slotConnected()
{
    //qDebug() << "Connection established";
}

void AsyncClient::slotError(QAbstractSocket::SocketError err)
{
    QString error = "An error occured: ";

    if (err == QAbstractSocket::HostNotFoundError)
        error += "host not found.";
    else if (err == QAbstractSocket::RemoteHostClosedError)
        error += "remote host is closed.";
    else if (err == QAbstractSocket::ConnectionRefusedError)
        error += "connection refused.";

    qDebug() << err;
    this->socket->close();
    this->socket->connectToHost(QHostAddress::LocalHost, this->hostPort);
}

void AsyncClient::sendData(QByteArray data)
{
    this->socket->write(data);
}

void AsyncClient::slotReadyRead()
{
    QByteArray data = this->socket->readAll();
    QJsonObject response = QJsonDocument::fromJson(data).object();
    if (response["username"] != QJsonValue::Undefined)
        emit updUsername("You're logged in as "+response["username"].toString());
    if (response["chat_membership"] != QJsonValue::Undefined)
        emit updChatList(response["chat_membership"].toArray());
    if (response["newest_messages"] != QJsonValue::Undefined)
        emit updNewestMessages(response["newest_messages"].toObject()["chat_id"].toInt(),
                               response["newest_messages"].toObject()["messages"].toArray());
}
