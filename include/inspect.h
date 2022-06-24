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

void reset_kaller_wd(const std::string &wd);
int write_syzkaller_config(const Bug_Info &, const InspectorConfig &, const VMConfig &, Port_Info &);

Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int);
Syzkaller_Result fuzz_loop(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, int, const VMConfig &, Port_Info &);

#endif