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
