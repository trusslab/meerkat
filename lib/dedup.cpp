#include <dedup.h>
#include <file_api.h>
#include <my_string.h>
#include <report.h>

#include <iostream>
#include <map>
#include <string>

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
        // TODO: parse the report here
        stack.clear();
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

bool compare_stack_traces(const BugAlias &bug1, const BugAlias &bug2)
{
    // if either stack is uncomparable, return false (i.e. don't use stack comparison).
    if (!bug1.has_stack() || !bug2.has_stack())
        return false;
    
    // TODO: figure out the comparison algorithm
    return false;
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
