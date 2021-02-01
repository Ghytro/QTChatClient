#ifndef CHATCREATIONDIALOG_H
#define CHATCREATIONDIALOG_H

#include <QDialog>
#include <QtWidgets>
#include "client.h"

namespace Ui {
class ChatCreationDialog;
}

class ChatCreationDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ChatCreationDialog(QWidget *parent = nullptr);
    ~ChatCreationDialog();

public slots:
    void slotSetErrorLabelText(QString);

private slots:
    void on_pushButtonAddMember_released();

    void on_listWidgetMembers_itemActivated(QListWidgetItem *item);

    void on_pushButtonDeleteMember_released();

    void on_pushButtonReset_released();

    void on_pushButtonOK_released();

    void on_pushButtonCancel_released();

    void on_lineEditAddMember_textChanged(const QString &arg1);

signals:
    void sendDataFromClient(QByteArray);

private:
    Ui::ChatCreationDialog *ui;
    Client *client;
};

#endif // CHATCREATIONDIALOG_H
