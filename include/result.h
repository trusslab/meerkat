#ifndef RESULT_H
#define RESULT_H

#include <string>
#include <vector>

class Crash_Report
{
public:
    std::string name;
    int time;

    Crash_Report(const std::string &n, int t)
        : name(n), time(t)
    { return; }

    Crash_Report()
    { Crash_Report("", 0); }
    
};

class Syzkaller_Result
{
public:
    bool found;                             // was the bug found
    int bad_crashes;
    int ttf;                                // time to find
    std::vector<Crash_Report> reports;      // a list of all the bugs found
};

class Test_Result
{
public:
    bool found;                             // was the bug found
    bool stable;                            // is the result stable
    bool retry;                             // signal to retry this fuzzing session after rebuilding
    int suggest_ttf;                        // sugested max ttf for this bug
    std::vector<Syzkaller_Result> attempts; // results for each time Syzkaller fuzzed
};

bool fuzz_is_crash_in(const std::string &, const std::vector<std::string> &);
int cr_find(const std::string &, const std::vector<Crash_Report> &);
bool fuzz_is_bad_crash(const std::string &);
bool result_is_stable(const Test_Result &);

#endif
