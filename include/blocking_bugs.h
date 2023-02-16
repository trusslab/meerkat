#include <result.h>

#include <string>
#include <vector>

class Blocking_Bugs
{
public:
    std::vector<Crash_Report> bugs;
    int count_not_found;

    Blocking_Bugs()
        : count_not_found(0)
    { return; }

    Blocking_Bugs(std::vector<Crash_Report> b, int c)
        : bugs(b), count_not_found(c)
    { return; }

    void count_blocking_bugs(const Test_Result &);
    std::vector<std::string> list_blocking_bugs();
};
