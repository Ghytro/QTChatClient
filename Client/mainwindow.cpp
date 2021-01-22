#include "mainwindow.h"
#include "chatwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
      ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    client = new Client(9999);

    connect(client, SIGNAL(setErrorLabelText(QString)),
            this,   SLOT(slotSetErrorLabelText(QString)));

    connect(client, SIGNAL(switchToChatWindow()),
            this,   SLOT(slotSwitchToChatWindow()));
}

MainWindow::~MainWindow()
{
    delete ui;
    client->deleteLater();
}

void MainWindow::slotSetErrorLabelText(QString str)
{
    this->ui->labelError->setText(str);
}

void MainWindow::slotSwitchToChatWindow()
{
    this->hide();
    ChatWindow *cw = new ChatWindow(this);
    cw->show();
}

void MainWindow::on_pushButtonSubmit_released()
{
    QString error = "";
    if (this->ui->lineEditUsername->text() == "")
        error = "Username field is empty";
    else if (this->ui->lineEditPassword->text() == "")
        error = "Password field is empty";
    else if (this->ui->lineEditUsername->text().contains(' '))
        error = "Username cannot contain spaces";
    if (error != "")
    {
        this->ui->labelError->setText(QStringLiteral("<p color=\"red\">%1</p>").arg(error));
        return;
    }

    QJsonObject query, params;
    query.insert("method", "access_token.change");
    params.insert("username", this->ui->lineEditUsername->text());
    params.insert("password", this->ui->lineEditPassword->text());
    query.insert("params", QJsonValue::fromVariant(params));
    QByteArray data = QJsonDocument(query).toJson();
    this->client->sendData(data);
}

void MainWindow::on_pushButtonCreateAccount_released()
{
    QString error = "";
    if (this->ui->lineEditUsername->text() == "")
        error = "Username field is empty";
    else if (this->ui->lineEditPassword->text() == "")
        error = "Password field is empty";
    if (error != "")
    {
        this->ui->labelError->setText(QStringLiteral("<p color=red>%1</p>").arg(error));
        return;
    }

    QJsonObject query, params;
    query.insert("method", "user.create");
    params.insert("username", this->ui->lineEditUsername->text());
    params.insert("password", this->ui->lineEditPassword->text());
    query.insert("params", QJsonValue::fromVariant(params));
    QByteArray data = QJsonDocument(query).toJson();
    this->client->sendData(data);
}
