#include <dedup.h>
#include <file_api.h>
#include <levenshtein.h>
#include <my_string.h>
#include <report.h>

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <iomanip>

// Deduplication workflow:
// Input Materials:
    // File layout similar to wd-kaller/crashes/

// bugs/
    // primary
        // description
        // report[0-9]*
    // alias# (any other name can work)
        // description
        // report[0-9]*

    // report is optional
    // can have any number of dirs, each needs a description
    // names are important, other names will be ignored

// Deduplication:
    // symbolize outputs one or more bugs
    // read bug descriptions and check against given bug descriptions using crash_name_eq
        // (if matches, stop here)
    // if a report was given as input, compare symbolize report to given report (top 5 functions, 4 must match, and/or crash type)

BugAlias::BugAlias(const std::string &p)
{
    path = ends_with(p, "/") ? p : p + "/";
    name.clear();
    stack.clear();
    
}

BugAlias::BugAlias(const BugAlias &a)
{
    path = a.path;
    name = a.name;
    stack = a.stack;
}

int BugAlias::init(bool full)
{
    // list the directory, find "description", any number of "report([0-9]*)", choose one
    std::vector<std::string> files = list_dir(path);

    std::string description, report;
    for (std::string f : files)
    {
        // get the filename only
        int pos = f.find_last_of('/');
        if (pos != std::string::npos)
            f = f.substr(pos + 1);
        
        if (f == "description")
        {
            description = path + f;
        }
        else if (starts_with(f, "report"))
        {
            report = path + f;
        }
    }

    if (!description.empty())
    {
        // description only has one line and it is the bug name.
        std::vector<std::string> lines;
        load_file(description, lines);
        name = lines.front();
    }
    else
        return -1;

    if (!full)
        return 0;

    if (!report.empty())
    {
        stack.clear();
        return parse_report(report, stack);
    }

    return 0;
}

// Return the part of the name before " in ".
// We'll handle matching similar types later.
std::string BugAlias::crash_type() const
{
    if (name.empty())
        return "";

    int pos = name.find(" in ");
    if (pos != std::string::npos)
        return name.substr(0, pos);
    return "";
}

// Return the part of the name after " in ".
std::string BugAlias::crash_function() const
{
    if (name.empty())
        return "";

    int pos = name.find(" in ");
    if (pos != std::string::npos)
        return name.substr(pos + 4);
    return "";
}

std::string BugAlias::debug() const
{
    std::stringstream ss;
    ss << name << std::endl;
    ss << path << std::endl;

    if (has_stack())
    {
        ss << std::endl;
        for (std::string func : stack)
            ss << func << std::endl;
    }

    return ss.str();
}

Crash_Report::Crash_Report(const BugAlias &a, int t, int c)
{
    alias = a;
    time = t;
    count = c;
}

Crash_Report::Crash_Report()
{
    alias = BugAlias();
    time = 0;
    count = 0;
}

bool Test_Result::is_stable() const
{
    int count = 0;
    if (found)
        return true;

    for (Syzkaller_Result sr : attempts)
        if (sr.bad_crashes > 0)
            count++;
    
    return (count < attempts.size() / 2);
}

#define COMPARISON_THRESHOLD 4
#define MAX_STACK_COMPARE 5

// In a simple comparison, 
bool simple_stack_comparison(const BugAlias &bug1, const BugAlias &bug2)
{
    unsigned int count = 0;
    for (int i = 0; i < bug1.stack.size() && i < bug2.stack.size() && i < MAX_STACK_COMPARE; i++)
        if (bug1.stack.at(i) == bug2.stack.at(i))
            count++;

    return count >= COMPARISON_THRESHOLD;
}

bool mutation_stack_comparison(const BugAlias &bug1, const BugAlias &bug2)
{
    bool maybe_change = false;
    int additions = 0, removals = 0, changes = 0, correct = 0;
    int j = 0, k = 0;
    for (int i = 0; i < bug1.stack.size() && i < MAX_STACK_COMPARE; i++)
    {
        for (k = j; k < bug2.stack.size() && bug1.stack.at(i) != bug2.stack.at(k); k++);
        if (k >= bug2.stack.size())
        {
            maybe_change = true;
            removals++;
            continue;
        }
        correct++;
        if (maybe_change)
        {
            maybe_change = false;
            if (k - j >= 1)
            {
                changes++;
                removals--;
            }
            if (k - j > 1)
            {
                additions += k - j - 1;
            }
            // if k == j, it was a removal.
        }
        else
        {
            if (k > j)
                additions += k - j;
        }
        j = k + 1;
    }

    // The threshold here could vary quite a bit. I'll need to tune it.
    return (correct + changes >= COMPARISON_THRESHOLD) && changes <= 1 && additions + removals <= 1;
}

#define LEVENSHTEIN_THRESHOLD 0.8

bool levenshtein_stack_comparison(const BugAlias &bug1, const BugAlias &bug2)
{
    return (1 - levenshtein_vec_norm(bug1.stack, bug2.stack)) >= LEVENSHTEIN_THRESHOLD;
}

bool compare_stack_traces(const BugAlias &bug1, const BugAlias &bug2)
{
    // if either stack is uncomparable, return false (i.e. don't use stack comparison).
    if (!bug1.has_stack() || !bug2.has_stack())
        return false;

    return simple_stack_comparison(bug1, bug2);
}

std::string cluster_crash_type(const std::string &ct)
{
    std::map<std::string, std::string> clustering = {
        // KASAN: use-after-free
        {"KASAN: slab-use-after-free Read", "KASAN: use-after-free"},
        {"KASAN: slab-use-after-free Write", "KASAN: use-after-free"},
        {"KASAN: slab-use-after-free", "KASAN: use-after-free"},
        {"KASAN: use-after-free Write", "KASAN: use-after-free"},
        {"KASAN: use-after-free Read", "KASAN: use-after-free"},
        // KASAN: slab-out-of-bounds
        {"KASAN: slab-out-of-bounds Write", "KASAN: slab-out-of-bounds"},
        {"KASAN: slab-out-of-bounds Read", "KASAN: slab-out-of-bounds"},
        {"KASAN: slab-out-of-bounds", "KASAN: slab-out-of-bounds"},
        // KASAN: stack-out-of-bounds
        {"KASAN: stack-out-of-bounds Read", "KASAN: stack-out-of-bounds"},
        {"KASAN: stack-out-of-bounds Write", "KASAN: stack-out-of-bounds"},
        {"KASAN: stack-out-of-bounds", "KASAN: stack-out-of-bounds"},
        // null-ptr-deref
        {"KASAN: null-ptr-deref Read", "null-ptr-deref"},
        {"KASAN: null-ptr-deref Write", "null-ptr-deref"},
        {"KASAN: null-ptr-deref", "null-ptr-deref"},
        {"general protection fault", "null-ptr-deref"},
        {"BUG: unable to handle kernel NULL pointer dereference", "null-ptr-deref"},
    };

    if (clustering.find(ct) != clustering.end())
        return clustering.at(ct);
    return ct;
}

bool compare_crash_types(const BugAlias &bug1, const BugAlias &bug2)
{
    return cluster_crash_type(bug1.crash_type()) == cluster_crash_type(bug2.crash_type());
}

bool compare_crash_names(const BugAlias &bug1, const BugAlias &bug2)
{
    return bug1.crash_function() == bug2.crash_function() && compare_crash_types(bug1, bug2);
}

// determine if the given crash is one of the know aliases. Return true or false
bool deduplicate(const BugAlias &crash, const std::vector<BugAlias> &aliases)
{
    for (BugAlias alias : aliases)
        if (compare_crash_names(crash, alias))
            return true;
    
    for (BugAlias a : aliases)
        if (compare_stack_traces(crash, a) && compare_crash_types(crash, a))
            return true;

    return false;
}
