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
Test_Result fuzz_loop_finding(Environment &, const Bug_Info &, bool);
Test_Result poc_loop_finding(Environment &, const Bug_Info &);

// Runs syzkaller fuzztimes times. Returns the culmination
// of the results.
Test_Result fuzz_loop(Environment &, const Bug_Info &, bool);
Test_Result poc_loop(Environment &, const Bug_Info &);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
//bool check_faulty_result(const Bug_Info &);

#endif
