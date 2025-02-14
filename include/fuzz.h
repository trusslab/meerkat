#ifndef FUZZ_H
#define FUZZ_H

#include <environment.h>
#include <date.h>
#include <result.h>

#include <fstream>
#include <string>
#include <vector>

std::string get_crash_name(const std::string &);

// Run syz-repro, return true if successful.
// Takes env, the destination prog file, and the crash log
bool run_syz_repro(const Environment &, const std::string &, const std::string &);

// Runs Syzkaller fuzztimes times. Returns the new max time
// to use. Intended to be run on the finding commit.
Test_Result fuzz_loop_finding(Environment &);

// Runs syzkaller fuzztimes times. Returns the culmination
// of the results.
Test_Result fuzz_loop(Environment &);
Test_Result poc_loop(Environment &);

Syzkaller_Result symbolize(Environment &, const std::string &);

// Checks against heuristics to see if the resulting kernel
// commit is faulty. Returns true if it is.
//bool check_faulty_result();

#endif
