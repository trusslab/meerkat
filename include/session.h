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
};

bool already_fuzzed(const std::vector<Session> &, const Session &);
int session_get_result(const std::vector<Session> &, const Session &);
int session_get_stable(const std::vector<Session> &, const Session &);

#endif
