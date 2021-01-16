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

class UserNotFoundException: public QException
{
public:
    void raise() const override {throw *this;}
    UserNotFoundException* clone() const override {return new UserNotFoundException();}
};

class ChatIsNotVisibleException: public QException
{
public:
    void raise() const override {throw *this;}
    ChatIsNotVisibleException* clone() const override {return new ChatIsNotVisibleException(*this);}
};

class TokenDoesNotExistException: public QException
{
public:
    void raise() const override {throw *this;}
    TokenDoesNotExistException* clone() const override {return new TokenDoesNotExistException(*this);}
};

class UserIsAlreadyInChatException: public QException
{
public:
    void raise() const override {throw *this;}
    UserIsAlreadyInChatException* clone() const override {return new UserIsAlreadyInChatException(*this);}
};

#endif // EXCEPTIONS_H
