#ifndef ASYNCCLIENTMANAGER_H
#define ASYNCCLIENTMANAGER_H

#include "asyncclient.h"
class AsyncClientManager: public QObject
{
    Q_OBJECT
public:
    AsyncClientManager(AsyncClient*);
    virtual ~AsyncClientManager();

public slots:
    void start();
    void stop();
    void setCurrentChatID(int);
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
