#ifndef VERSION_H
#define VERSION_H

#include <date.h>

#include <string>

// A class to storeinformation about versions of 
// gcc, linux, or syzkaller (or anything else).
// Stores an identifying value and the assiciated
// date.
class Version
{
public:
    std::string name;
    Date date;
};

#endif