#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    if (QFile("token").exists())
        w.slotSwitchToChatWindow();
    return a.exec();
}
