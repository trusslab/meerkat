#ifndef SESSION_H
#define SESSION_H

#include <version.h>

#include <vector>

class Session
{
public:
    Version kernel;
    Version syzkaller;
    Version syz_template;
    bool found;

    Session()
    { return; }

    Session(const Version &k, const Version &syz, const Version &temp, bool f)
        : kernel(k), syzkaller(syz), syz_template(temp), found(f)
    {
        return;
    }
};

bool already_fuzzed(const std::vector<Session> &, const Session &);
int get_result(const std::vector<Session> &, const Session &);

#endif
