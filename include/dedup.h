#ifndef DEDUP_H
#define DEDUP_H

#include <vector>
#include <string>

class BugAlias
{
public:
    std::string name;                       // raw from description
    std::vector<std::string> stack;         // stack parsed from report
    std::string path;                       // path to the bug info

    BugAlias(const std::string & = "");
    BugAlias(const BugAlias &);

    int init(bool = true);

    bool has_stack() const
    { return !stack.empty(); }

    std::string crash_type() const;
    std::string crash_function() const;
};

class Crash_Report
{
public:
    BugAlias alias;                         // The bug identifier
    int time;                               // when the bug was found
    int count;                              // how many times was the bug found in the same increment

    Crash_Report(const BugAlias &a, int t, int c);
    Crash_Report();
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

    bool is_stable() const;
};

bool deduplicate(const BugAlias &, const std::vector<BugAlias> &);

#endif