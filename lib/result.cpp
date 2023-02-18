#include <result.h>

#include <vector>
#include <string>

using namespace std;

// Quick and dirty search function for string vectors
bool fuzz_is_in(const string &s, const vector<string> &v)
{
    for (string str : v)
        if (str == s)
            return true;
    
    return false;
}

int cr_find(const string &s, const vector<Crash_Report> &v)
{
    int i = 0;
    for (i = 0; i < v.size() && v.at(i).name != s; i++);
    return i < v.size() ? i : -1;
}

bool fuzz_is_bad_crash(const string &crash_name)
{
    return crash_name == "suppressed report" || crash_name == "panic: disabled syscall"
            || crash_name == "lost connection to test machine" || crash_name.find("SYZFATAL") != string::npos
            || crash_name.find("SYZFAIL:") != string::npos || crash_name == "boot failure";
}

bool result_is_stable(const Test_Result &result)
{
    int count = 0;
    if (result.found)
        return true;

    for (Syzkaller_Result sr : result.attempts)
        if (sr.bad_crashes > 0)
            count++;
    
    return (count < result.attempts.size() / 2);
}
