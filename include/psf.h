#ifndef PSF_H
#define PSF_H

#include <bug_info.h>

#include <string>
#include <vector>

class PSF_Bug
{
public:
    std::string name;
    std::vector<std::string> fixes;

    PSF_Bug()
    { return; }

    PSF_Bug(const std::string &n, const std::vector<std::string> & f)
        : name(n), fixes(f)
    { return; }
};

class PSF_Fix
{
public:
    std::string commit;
    std::vector<std::string> bugsFixed;

    PSF_Fix(const std::string & c)
        : commit(c)
    { return; }
};

// calls sed to clean up the dump file
void trim_syzbot_fixes(const std::string &);

// parses the fixes out of the dump file and returns
// the duplicate bug names related to this bug.
std::vector<std::string> parse_syzbot_fixes(const std::string &, const std::string &, std::vector<std::string> &);
std::vector<std::string> parse_manual_duplicates(const std::string &, const std::string &, std::vector<std::string> &);

std::vector<std::string> gather_duplicates(const Bug_Info &bug);

#endif
