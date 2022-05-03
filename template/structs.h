#ifndef STRUCTS_H
#define STRUCTS_H
#include <string>
#include <vector>

class ParseType
{
public:
    std::string name;
    char type;          // R T D S F
    std::string text;
    std::vector<std::string> depend;
    std::vector<std::string> args;

    ParseType(const std::string & n, const char t, const std::string & tx)
        : name(n), type(t), text(tx)
    { return; }

    ParseType()
    { return; }
};

class Syscall : public ParseType
{
public:
    std::vector<std::string> returnType;

    Syscall(const std::string & n, const char t, const std::string & tx)
        : ParseType(n, t, tx)
    { return; }

    Syscall()
    { return; }
};

#endif
