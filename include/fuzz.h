#ifndef FUZZ_H
#define FUZZ_H

#include <bug_info.h>
#include <inspector_config.h>
#include <date.h>
#include <result.h>

#include <fstream>
#include <string>
#include <vector>

// deletes the syzkaller working directory and
// recreates it.
void reset_kaller_wd(const std::string &wd);

// runs syzkaller once. Returns a data structure with
// time to find and bugs found.
Syzkaller_Result run_syzkaller(const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, bool = true);

// Runs Syzkaller fuzztimes times. Returns the new max time
// to use. Intended to be run on the finding commit.
Test_Result fuzz_loop_finding(std::ofstream &, const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const int, const VMConfig &, Port_Info &, const Date &, bool = true, bool = true);

// Runs syzkaller fuzztimes times. Returns the culmination
// of the results.
Test_Result fuzz_loop(std::ofstream &, const Bug_Info &, const InspectorConfig &, const std::vector<std::string> &, const int, const int, const VMConfig &, Port_Info &, const Date &, bool = true);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
bool check_faulty_result(const Bug_Info &);

#endif
