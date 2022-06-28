#include <version.h>
#include <date.h>

#include <vector>
#include <string>

using namespace std;

bool Version::operator==(const Version &other)
{
    return name == other.name;
}

bool Version::operator!=(const Version &other)
{
    return name != other.name;
}

// returns the most recent version on or before the given date
Version get_version_by_date(const vector<Version> &versions, const Date &date)
{
    for (int i = 0; i < versions.size(); i--)
    {
        if (versions.at(i).date <= date)
            return versions.at(i);
    }

    return versions.back();
}

int get_index_by_name(const vector<Version> &versions, const string &name)
{
    int i;
    for (i = 0; i < versions.size() && name != versions.at(i).name; i++);
    return i < versions.size() ? i : -1;
}

// get the index of the first (oldest) version on or after the given date
int get_starting_index(const std::vector<Version> &versions, const Date &d)
{
    int i;
    for (i = versions.size() - 1; i >= 0 && versions.at(i).date < d; i--);
    return i < 0 ? 0 : i;
}

// get the index of the last (most recent) version on or before the given date
int get_ending_index(const std::vector<Version> &versions, const Date &d)
{
    int i;
    for (i = 0; i < versions.size() && versions.at(i).date > d; i++);
    return i;
}
