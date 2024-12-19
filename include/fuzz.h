#ifndef FUZZ_H
#define FUZZ_H

#include <bug_info.h>
#include <environment.h>
#include <date.h>
#include <result.h>

#include <fstream>
#include <string>
#include <vector>

// Runs Syzkaller fuzztimes times. Returns the new max time
// to use. Intended to be run on the finding commit.
Test_Result fuzz_loop_finding(std::ofstream &, Environment &, const Bug_Info &, const Date &, bool);
Test_Result repro_loop_finding(std::ofstream &, Environment &, const Bug_Info &, const Date &);

// Runs syzkaller fuzztimes times. Returns the culmination
// of the results.
Test_Result fuzz_loop(std::ofstream &, Environment &, const Bug_Info &, const Date &, bool);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
bool check_faulty_result(const Bug_Info &);

#endif
