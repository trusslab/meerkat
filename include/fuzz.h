#ifndef FUZZ_H
#define FUZZ_H

#include <bug_info.h>
#include <inspector_config.h>
#include <date.h>

#include <string>
#include <vector>

class Syzkaller_Result
{
public:
    bool found;                             // was the bug found
    int ttf;                                // time to find. this ios the only time that matters
    std::vector<std::string> bugsfound;     // a list of all the bugs found
};

// deletes the syzkaller working directory and
// recreates it.
void reset_kaller_wd(const std::string &wd);

// runs syzkaller once. Returns a data structure with
// time to find and bugs found.
Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, bool = true);

// Runs Syzkaller FUZZTIMES times. Returns the new max time
// to use. Intended to be run on the finding commit.
Syzkaller_Result fuzz_loop_finding(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const VMConfig &, Port_Info &, const Date &, bool = true, bool = true);

// Runs syzkaller FUZZTIMES times. Returns the culmination
// of the results.
Syzkaller_Result fuzz_loop(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const VMConfig &, Port_Info &, const Date &, bool = true);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
bool check_faulty_result(const Bug_Info &, const std::vector<int> &, int);

#endif
