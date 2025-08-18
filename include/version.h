#ifndef VERSION_H
#define VERSION_H

#include <date.h>

#include <string>
#include <vector>

// A class to store information about versions of 
// gcc, linux, or syzkaller (or anything else).
// Stores an identifying value and the assiciated
// date.
class Version
{
public:
    std::string tag;
    std::string id;
    Date date;
    bool skipped;

    Version()
        : skipped(false)
    { return; }

    Version(const std::string &n, const Date &d)
        : id(n), date(d), skipped(false)
    { return; }

    Version(const std::string &t, const std::string &n, const Date &d)
        : tag(t), id(n), date(d), skipped(false)
    { return; }

    std::string string() const;

    bool operator==(const Version &);
    bool operator!=(const Version &);
};

#endif
