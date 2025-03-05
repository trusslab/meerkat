#include <my_string.h>
#include <result.h>

#include <vector>
#include <string>

using namespace std;

bool crash_name_eq(const string &c1, const string &c2)
{
    vector<string> c1_spl = split(c1, ' ');
    vector<string> c2_spl = split(c2, ' ');
    return c1_spl.front() == c2_spl.front() && c1_spl.back() == c2_spl.back();
}

// Quick and dirty search function for string vectors
bool fuzz_is_crash_in(const string &crash, const vector<string> &dups)
{
    for (string dup : dups)
        if (crash_name_eq(crash, dup))
            return true;
    
    return false;
}

int cr_find(const string &s, const vector<Crash_Report> &v)
{
    int i = 0;
    for (i = 0; i < v.size(); i++)
        if (crash_name_eq(s, v.at(i).name))
            break;
    return i < v.size() ? i : -1;
}

bool fuzz_is_bad_crash(const string &crash_name)
{
    return crash_name.find("SYZFATAL") != string::npos
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
