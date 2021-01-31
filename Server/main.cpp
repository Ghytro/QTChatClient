#include <QCoreApplication>
#include "tcpserver.h"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    Server server(9999);

    // (int i = 0; i < 40; ++i)
        //Server::debugSendMessage(0, "flood0", 1);

    return a.exec();
}
