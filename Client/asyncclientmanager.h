#ifndef ASYNCCLIENTMANAGER_H
#define ASYNCCLIENTMANAGER_H

#include <QtCore>
#include <QTcpSocket>
#include <QTcpServer>

class AsyncClient;

class AsyncClientManager: public QObject
{
    Q_OBJECT
public:
    AsyncClientManager(AsyncClient*);
    virtual ~AsyncClientManager();

public slots:
    void start();
    void stop();
    void slotSetCurrentChatID(int);
    void addPendingMessage(QString);

signals:
    void stopped();
    void sendDataFromClient(QByteArray);

private:
    AsyncClient *client;
    bool isWorking = false;
    int currentChatID = -1;
    static const unsigned latestMessagesNum = 200;
    QMap<int, QQueue<QString>> pendingMessages;
};

#endif // ASYNCCLIENTMANAGER_H
