#include <QCoreApplication>
#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server server(9999);

    for (size_t i = 0; i < 1000; ++i)
        Server::debugSendMessage(0, QStringLiteral("message%1").arg(i), 1);

    qDebug() << "over";

    return a.exec();
}
