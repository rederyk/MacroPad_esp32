#ifndef COMMAND_H
#define COMMAND_H

class Command {
public:
    virtual ~Command() = default;
    virtual void press() = 0;
    virtual void release() = 0;
};

#endif // COMMAND_H