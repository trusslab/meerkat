#include <session.h>

#include <vector>

using namespace std;

bool already_fuzzed(const vector<Session> &sessions, const Session &this_session)
{
    for (Session s : sessions)
    {
        if (s.kernel.name == this_session.kernel.name &&
            s.syz_template.name == this_session.syz_template.name &&
            s.syzkaller.name == this_session.syzkaller.name)
        {
            return true;
        }
    }

    return false;
}

int get_result(const vector<Session> &sessions, const Session &this_session)
{
    for (Session s : sessions)
    {
        if (s.kernel.name == this_session.kernel.name &&
            s.syz_template.name == this_session.syz_template.name &&
            s.syzkaller.name == this_session.syzkaller.name)
        {
            return s.found ? 1 : 0;
        }
    }

    return -1;
}
