#include "chatcreationdialog.h"
#include "ui_chatcreationdialog.h"

ChatCreationDialog::ChatCreationDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ChatCreationDialog)
{
    ui->setupUi(this);
    this->setWindowFlag(Qt::WindowContextHelpButtonHint, false);

    this->client = new Client(9999);

    connect(this->client, SIGNAL(setErrorLabelText(QString)),
            this,         SLOT(slotSetErrorLabelText(QString)));

    connect(this,         SIGNAL(sendDataFromClient(QByteArray)),
            this->client, SLOT(sendData(QByteArray)));

    connect(this->client, SIGNAL(chatSuccessfullyCreated()),
            this,         SLOT(close()));
}

ChatCreationDialog::~ChatCreationDialog()
{
    delete ui;
    this->client->deleteLater();
}

void ChatCreationDialog::on_pushButtonAddMember_released()
{
    QString username = this->ui->lineEditAddMember->text();
    if (username.contains(" ") || username == "") //wrong username
        return;
    this->ui->listWidgetMembers->addItem(username);
    this->ui->lineEditAddMember->clear();
}

void ChatCreationDialog::on_listWidgetMembers_itemActivated(QListWidgetItem *item)
{
    this->ui->pushButtonDeleteMember->setEnabled(true);
}

void ChatCreationDialog::on_pushButtonDeleteMember_released()
{
    QList<QListWidgetItem*> selectedItems = ui->listWidgetMembers->selectedItems();
    for (QListWidgetItem *i: selectedItems)
        delete this->ui->listWidgetMembers->takeItem(this->ui->listWidgetMembers->row(i));
    this->ui->pushButtonDeleteMember->setEnabled(false);
}


void ChatCreationDialog::on_pushButtonReset_released()
{
    this->ui->lineEditChatName->clear();
    this->ui->listWidgetMembers->clear();
    this->ui->pushButtonDeleteMember->setEnabled(false);
    this->ui->lineEditAddMember->clear();
}

void ChatCreationDialog::on_pushButtonOK_released()
{
    QString chatName = this->ui->lineEditChatName->text();
    if (chatName == "")
    {
        this->slotSetErrorLabelText("Enter the chat name");
        return;
    }


    QJsonArray chatMembers;
    for (int row = 0; row < this->ui->listWidgetMembers->count(); ++row)
        chatMembers.append(this->ui->listWidgetMembers->item(row)->text());

    if (chatMembers.isEmpty())
    {
        this->slotSetErrorLabelText("Invite at least one person");
        return;
    }

    QString accessToken;
    QFile tokenFile("token");
    if (!tokenFile.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "Unable to open file with token";
        return;
    }
    accessToken = tokenFile.readAll();
    tokenFile.close();

    QJsonObject query, params;
    query.insert("method", "chat.create");
    params.insert("name", QJsonValue::fromVariant(chatName));
    params.insert("members", QJsonValue::fromVariant(chatMembers));
    params.insert("is_visible", QJsonValue::fromVariant(true)); //add visibility specifying later
    params.insert("access_token", QJsonValue::fromVariant(accessToken));
    query.insert("params", params);

    emit sendDataFromClient(QJsonDocument(query).toJson());
}

void ChatCreationDialog::slotSetErrorLabelText(QString text)
{
    this->ui->labelError->setText(text);
}

void ChatCreationDialog::on_pushButtonCancel_released()
{
    this->close();
}
