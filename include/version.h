#ifndef VERSION_H
#define VERSION_H

#include <date.h>

#include <string>
#include <vector>

#define STABLE_INFER_SIZE 50

// A class to store information about versions of 
// gcc, linux, or syzkaller (or anything else).
// Stores an identifying value and the assiciated
// date.
class Version
{
public:
    std::string tag;
    std::string id;
    Date date;
    bool skipped;

    Version()
        : skipped(false)
    { return; }

    Version(const std::string &n, const Date &d)
        : id(n), date(d), skipped(false)
    { return; }

    Version(const std::string &t, const std::string &n, const Date &d)
        : tag(t), id(n), date(d), skipped(false)
    { return; }

    std::string string() const;

    bool operator==(const Version &);
    bool operator!=(const Version &);
};

class Version_p
{
public:
    Version v;
    std::vector<std::string> parents;
};

void infer_stability(std::vector<Version> &, bool = false);
std::vector<Version> get_only_stable(const std::vector<Version> &);

Version get_version_by_date(const std::vector<Version> &, const Date &);
Version get_stable_version_by_date(const std::vector<Version> &, const Date &);

int get_index_by_id(const std::vector<Version> &, const std::string &, const int pos = 0);

int get_index_by_id(const std::vector<Version_p> &, const std::string &, const int pos = 0);

// get the index of the first (oldest) version on or after the given date
int get_starting_index(const std::vector<Version> &, const Date &);

// get the index of the last (most recent) version on or before the given date
int get_ending_index(const std::vector<Version> &, const Date &);

#endif
