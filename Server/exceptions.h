#ifndef EXCEPTIONS_H
#define EXCEPTIONS_H

#include <QException>

class UserIsNotMemberOfChatException: public QException
{
public:
    void raise() const override {throw *this;}
    UserIsNotMemberOfChatException* clone() const override {return new UserIsNotMemberOfChatException(*this);}
};

class UserIsNotAdminException: public QException
{
public:
    void raise() const override {throw *this;}
    UserIsNotAdminException* clone() const override {return new UserIsNotAdminException();}
};

class ChatIsNotVisibleException: public QException
{
public:
    void raise() const override {throw *this;}
    ChatIsNotVisibleException* clone() const override {return new ChatIsNotVisibleException(*this);}
};

#endif // EXCEPTIONS_H
