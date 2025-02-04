#ifndef SESSION_H
#define SESSION_H

#include <version.h>

#include <vector>

class Session
{
public:
    Version kernel;
    bool found;
    bool stable;

    Session()
        : stable(true)
    { return; }

    Session(const Version &k, bool f)
        : kernel(k), found(f), stable(true)
    {
        return;
    }

    bool operator==(const Session &other)
    { return kernel == other.kernel; }

    bool operator<(const Session &other) const
    { return kernel.name < other.kernel.name; }
};

#endif
