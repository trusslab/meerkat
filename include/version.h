#ifndef VERSION_H
#define VERSION_H

#include <date.h>

#include <string>
#include <vector>

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

Version get_version_by_date(const std::vector<Version> &, const Date &);

#endif