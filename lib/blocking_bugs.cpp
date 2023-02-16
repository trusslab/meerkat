#include <blocking_bugs.h>
#include <result.h>

#include <string>
#include <vector>

using namespace std;

void Blocking_Bugs::count_blocking_bugs(const Test_Result &result)
{
    int i = 0;
    for (Syzkaller_Result sr : result.attempts)
    {
        if (sr.found)
            continue;
        
        count_not_found++;
        for (Crash_Report cr : sr.reports)
        {
            if (fuzz_is_bad_crash(cr.name))
                continue;

            i = cr_find(cr.name, bugs);
            if (i >= 0)
                bugs.at(i).time++;
            else
                bugs.push_back({cr.name, 1});
        }
    }
}

vector<string> Blocking_Bugs::list_blocking_bugs()
{
    vector<string> list;

    for (Crash_Report bb : bugs)
    {
        // this ratio may change. It is intended to capture bugs that are found
        // many times while the original bug was not.
        if (bb.time > count_not_found / 3)
            list.push_back(bb.name);
    }

    return list;
}
