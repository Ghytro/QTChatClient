#include "asyncclient.h"
#include "asyncclientmanager.h"

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
}

AsyncClient::~AsyncClient()
{
    this->socket->deleteLater();
}

void AsyncClient::setManager(AsyncClientManager *manager)
{
    this->manager = manager;
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
}

void AsyncClient::sendData(QByteArray data)
{
    this->socket->connectToHost(QHostAddress("192.168.50.19"), this->hostPort);
    this->socket->waitForConnected();
    this->socket->write(data);
}

void AsyncClient::slotReadyRead()
{
    QByteArray data = this->socket->readAll();
    this->socket->close();
    if (this->socket->state() != QAbstractSocket::UnconnectedState)
        this->socket->waitForDisconnected();
    QJsonObject response = QJsonDocument::fromJson(data).object();
    if (response.contains("username"))
        emit updUsername("You're logged in as "+response["username"].toString());
    if (response.contains("chat_membership"))
        emit updChatList(response["chat_membership"].toArray());
    if (response.contains("newest_messages"))
        emit updNewestMessages(response["newest_messages"].toObject()["chat_id"].toInt(),
                               response["newest_messages"].toObject()["messages"].toArray());
    emit gotReply();
}
