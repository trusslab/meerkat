#ifndef INSPECT_H
#define INSPECT_H

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
Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int, bool = true);

// Runs Syzkaller FUZZTIMES times. Returns the new max time
// to use. Intended to be run on the finding commit.
Syzkaller_Result fuzz_loop_finding(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int, const VMConfig &, Port_Info &, const Date &, bool = true);

// Runs syzkaller FUZZTIMES times. Returns the culmination
// of the results.
Syzkaller_Result fuzz_loop(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int, const VMConfig &, Port_Info &, const Date &, bool = true);

#endif
