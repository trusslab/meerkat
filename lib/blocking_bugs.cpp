#include <blocking_bugs.h>
#include <my_string.h>
#include <result.h>

#include <string>
#include <vector>

using namespace std;

void Blocking_Bugs::count_blocking_bugs(const Test_Result &result)
{
    for (Syzkaller_Result sr : result.attempts)
    {
        if (sr.found)
            continue;
        
        count_not_found++;
        for (Crash_Report cr : sr.reports)
        {
            if (fuzz_is_bad_crash(cr.name))
                continue;

            if (bugs.find(cr.name) != bugs.end())
                bugs.at(cr.name)++;
            else
                bugs.insert({cr.name, 1});
        }
    }
}

vector<string> Blocking_Bugs::list_blocking_bugs()
{
    vector<string> list;

    for (map<string, int>::iterator it = bugs.begin(); it != bugs.end(); it++)
    {
        // this ratio may change. It is intended to capture bugs that are found
        // many times while the original bug was not.
        if (it->second > count_not_found / 3)
            list.push_back(it->first);
    }

    return list;
}

vector<string> get_prominent_blocking_bugs(const Test_Result &result)
{
    map<string, int> bugs;
    for (Syzkaller_Result sr : result.attempts)
    {
        for (Crash_Report cr : sr.reports)
        {
            if (fuzz_is_bad_crash(cr.name))
                continue;

            if (bugs.find(cr.name) != bugs.end())
                bugs.at(cr.name)++;
            else
                bugs.insert({cr.name, 1});
        }
    }

    vector<string> list;
    for (map<string, int>::iterator it = bugs.begin(); it != bugs.end(); it++)
    {
        if (it->second > 20)
            list.push_back(it->first);
    }
    return list;
}
