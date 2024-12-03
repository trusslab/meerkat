#ifndef SESSION_H
#define SESSION_H

#include <version.h>

#include <vector>

class Session
{
public:
    Version kernel;
    Version syzkaller;
    bool found;
    bool stable;

    Session()
        : stable(true)
    { return; }

    Session(const Version &k, const Version &syz, bool f)
        : kernel(k), syzkaller(syz), found(f), stable(true)
    {
        return;
    }

    bool operator==(const Session &other)
    { return kernel == other.kernel && syzkaller == other.syzkaller; }

    bool operator<(const Session &other) const
    { return (kernel.name + syzkaller.name) < (other.kernel.name + other.syzkaller.name); }
};

#endif
