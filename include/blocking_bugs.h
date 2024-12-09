#ifndef BLOCKING_BUGS
#define BLOCKING_BUGS

#include <result.h>

#include <string>
#include <vector>
#include <map>

class Blocking_Bugs
{
public:
    std::map<std::string, int> bugs;
    int count_not_found;

    Blocking_Bugs()
        : count_not_found(0)
    { return; }

    Blocking_Bugs(std::vector<Crash_Report> b, int c)
    {
        count_not_found = c;
        for (Crash_Report cr : b)
            bugs.insert({cr.name, cr.time});
    }

    void count_blocking_bugs(const Test_Result &);
    std::vector<std::string> list_blocking_bugs();
};

std::vector<std::string> get_prominent_blocking_bugs(const Test_Result &);

#endif
