#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "client.h"
#include <QMainWindow>
#include <QtCore>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    Client *client;
    ~MainWindow();

private slots:
    void on_pushButtonSubmit_released();

    void on_pushButtonCreateAccount_released();

    void slotSetErrorLabelText(QString);

public slots:
    void slotSwitchToChatWindow();

private:
    Ui::MainWindow *ui;
};

#endif // MAINWINDOW_H
