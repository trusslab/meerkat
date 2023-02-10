#ifndef FUZZ_H
#define FUZZ_H

#include <bug_info.h>
#include <inspector_config.h>
#include <date.h>

#include <string>
#include <vector>

class Crash_Report
{
public:
    std::string name;
    int time;
};

class Syzkaller_Result
{
public:
    bool found;                             // was the bug found
    int ttf;                                // time to find
    std::vector<Crash_Report> reports;      // a list of all the bugs found
};

class Test_Result
{
public:
    bool found;                             // was the bug found
    int suggest_ttf;                        // sugested max ttf for this bug
    std::vector<Syzkaller_Result> attempts; // results for each time Syzkaller fuzzed
};

bool fuzz_is_in(const std::string &, const std::vector<std::string> &);

// deletes the syzkaller working directory and
// recreates it.
void reset_kaller_wd(const std::string &wd);

// runs syzkaller once. Returns a data structure with
// time to find and bugs found.
Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, bool = true);

// Runs Syzkaller fuzztimes times. Returns the new max time
// to use. Intended to be run on the finding commit.
Test_Result fuzz_loop_finding(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const int, const VMConfig &, Port_Info &, const Date &, bool = true, bool = true);

// Runs syzkaller fuzztimes times. Returns the culmination
// of the results.
Test_Result fuzz_loop(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const int, const VMConfig &, Port_Info &, const Date &, bool = true);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
bool check_faulty_result(const Bug_Info &, const std::vector<int> &, int);

#endif
