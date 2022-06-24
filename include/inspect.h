#ifndef INSPECT_H
#define INSPECT_H

#include <bug_info.h>
#include <inspector_config.h>

#include <string>
#include <vector>

class Syzkaller_Result
{
public:
    bool found;                             // was the bug found
    int ttf;                                // time to find
    std::vector<std::string> bugsfound;     // a list of all the bugs found
};

// deletes the syzkaller working directory and
// recreates it.
void reset_kaller_wd(const std::string &wd);

// writes the syzkaller config to the config file.
// also shifts the host port by one
int write_syzkaller_config(const Bug_Info &, const InspectorConfig &, const VMConfig &, Port_Info &);

// runs syzkaller once. Returns a data structure with
// time to find and bugs found.
Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int);

// Runs syzkaller FUZZTIMES times. Returns the culmination
// of the results.
Syzkaller_Result fuzz_loop(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int, const VMConfig &, Port_Info &);

#endif