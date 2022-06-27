#include <version.h>
#include <date.h>

#include <vector>
#include <string>

using namespace std;

Version get_version_by_date(const vector<Version> &versions, const Date &date)
{
    for (int i = 0; i < versions.size(); i--)
    {
        if (versions.at(i).date <= date)
            return versions.at(i);
    }

    return versions.back();
}