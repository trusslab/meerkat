#include <version.h>
#include <date.h>

#include <vector>
#include <string>
#include <iostream>

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
    int i;
    for (i = 0; i < versions.size() && date < versions.at(i).date; i++);
    i = (i >= versions.size() ? versions.size() - 1 : i);
    return versions.at(i);
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
    for (i = 0; i < versions.size() && d <= versions.at(i).date; i++);
    return i - 1;
}

// get the index of the last (most recent) version on or before the given date
int get_ending_index(const std::vector<Version> &versions, const Date &d)
{
    int i;
    for (i = 0; i < versions.size() && d < versions.at(i).date; i++);
    return (i >= versions.size() ? versions.size() - 1 : i);
}
