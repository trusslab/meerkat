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

// Mark commits between unstable versions as unstable.
// Infer some buffer around unstable ranges to be unstable as well.
// Works in-place on the vector
void infer_stability(vector<Version> &versions, bool use_buf)
{
    for (int i = 0; i < versions.size(); i++)
    {
        if (!versions.at(i).skipped)
            continue;

        // check forward to find the next unstable version within range
        int next_skip = i;
        for (int j = 1; j <= STABLE_INFER_SIZE && next_skip + j < versions.size(); j++)
        {
            if (versions.at(next_skip + j).skipped)
            {
                next_skip = next_skip + j;
                j = 1;
            }
        }

        // If there is a range of unstable commits, skip them all
        for (int j = i + 1; j < next_skip; j++)
            versions.at(j).skipped = true;

        if (!use_buf)
        {
            i = next_skip;
            continue;
        }

        // skip up and down to make a buffer.
        // Max buffer is STABLE_INFER_SIZE,
        // otherwise halfway between edge of unstable commits and edge of commit range.
        // Skipping here is a guess. Don't rule out the edges.

        // skipping down
        int buffer = i / 2;
        buffer = buffer > STABLE_INFER_SIZE - 1 ? STABLE_INFER_SIZE - 1 : buffer;
        for (int j = 1; j <= buffer && i - j > 0; j++)
            versions.at(i - j).skipped = true;

        // skipping up
        buffer = (versions.size() - 1 - next_skip) / 2;
        buffer = buffer > STABLE_INFER_SIZE - 1 ? STABLE_INFER_SIZE - 1 : buffer;
        for (int j = 1; j <= buffer && next_skip + j < versions.size() - 1; j++)
            versions.at(next_skip + j).skipped = true;

        i = next_skip + buffer; // i will get incremented on next loop
    }
}

vector<Version> get_only_stable(const vector<Version> &versions)
{
    vector<Version> versions2;
    for (int i = 0; i < versions.size(); i++)
        if (!versions.at(i).skipped)
            versions2.push_back(versions.at(i));
    return versions2;
}

// Returns the most recent version on or before the given date
Version get_version_by_date(const vector<Version> &versions, const Date &date)
{
    int i;
    for (i = 0; i < versions.size() && date < versions.at(i).date; i++);
    i = (i >= versions.size() ? versions.size() - 1 : i);
    return versions.at(i);
}

// Returns the most recent stable version on or before the given date.
// There must be at least one stable version remaining for this to work.
Version get_stable_version_by_date(const vector<Version> &versions, const Date &date)
{
    vector<Version> versions2 = versions;
    infer_stability(versions2);
    return get_version_by_date(get_only_stable(versions2), date);
}

int get_index_by_name(const vector<Version> &versions, const string &name, const int pos)
{
    int i = (pos < versions.size() ? pos : 0);
    for (; i < versions.size() && name != versions.at(i).name; i++);
    return i < versions.size() ? i : -1;
}

int get_index_by_name(const vector<Version_p> &versions, const string &name, const int pos)
{
    int i = (pos < versions.size() ? pos : 0);
    for (; i < versions.size() && name != versions.at(i).v.name; i++);
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
