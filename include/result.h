#ifndef RESULT_H
#define RESULT_H

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
    int bad_crashes;
    int ttf;                                // time to find
    std::vector<Crash_Report> reports;      // a list of all the bugs found
};

class Test_Result
{
public:
    bool found;                             // was the bug found
    bool stable;                            // is the result stable
    int suggest_ttf;                        // sugested max ttf for this bug
    std::vector<Syzkaller_Result> attempts; // results for each time Syzkaller fuzzed
};

bool fuzz_is_in(const std::string &, const std::vector<std::string> &);
int cr_find(const std::string &, const std::vector<Crash_Report> &);
bool fuzz_is_bad_crash(const string &);
bool result_is_stable(const Test_Result &);

#endif
