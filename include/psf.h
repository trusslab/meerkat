#ifndef PSF_H
#define PSF_H

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

void trim_syzbot_fixes(const std::string &);
std::vector<std::string> parse_syzbot_fixes(const std::string &, const std::string &);

#endif