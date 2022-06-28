#include <psf.h>
#include <shell_api.h>

#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cctype>

using namespace std;

// sed -i 1,/^[ ]*\[[0-9]*\]Title/ d snapshotfile
// sed -i /^$/q snapshotfile
void trim_syzbot_fixes(const string &filename)
{
    sed_i("1,/^[ ]*\\[[0-9]*\\]Title/ d", filename);
    sed_i("/^$/q", filename);
    return;
}

// find a fix name in a vector of fixes. -1 on fail
// will sort the array if it starts to take a while
int PSF_find(const string &s, const vector<PSF_Fix> &v)
{
    for (int i = 0; i < v.size(); i++)
        if (v.at(i).commit == s)
            return i;

    return -1;
}

bool PSF_is_in(const string &str, const vector<string> &vec)
{
    for (string s : vec)
        if (s == str)
            return true;
    
    return false;
}

// returns the bug name from the raw text displayed on syzbot's website
// "  [##]BUGNAME      C      Done   ..."
string PSF_getBugName(const string &line)
{
    // find the end of the link marker. The next char is the bug name
    int pos0 = line.find_first_of("]") + 1;

    // the first case of 2 or more spaces is the end of the bug name
    int pos1 = line.find("  ", pos0);

    return line.substr(pos0, pos1 - pos0);
}

// returns true if the character is a hex character (a-f, 0-9)
bool PSF_ishex(char a)
{
    a = tolower(a);
    return (a >= 'a' && a <= 'f') || (a >= '0' && a <= '9');
}

// returns the location of the first 12 char hash in a given string, starting at the given location, -1 on fail
int PSF_findHash(const string &s, int i = 0)
{
    int length = 0, pos0 = 0;
    for (; i < s.size() && length < 12; i++)
    {
        if (PSF_ishex(s.at(i)))
        {
            pos0 = (length == 0 ? i : pos0);
            length++;
        }
        else
            length = 0;
    }

    return (length < 12 ? -1 : pos0);
}

// returns a vector of all the fixes for a single bug/line
// "   .... HASH [##]COMMITNAME"
// "   .... HASH [##]COMMITNAME HASH [##]COMMITNAME..."
// some commits are listed twice in the same line: "COMMITNAME SAMECOMMITNAME"
// Could cause issues with matching fixing commits.
vector<string> PSF_getBugFixes(const string &line)
{
    int pos0, pos1;
    string s;
    vector<string> v;

    pos0 = PSF_findHash(line);

    while (pos0 >= 0)
    {
        // find the start of the commit name
        pos0 = line.find_first_of("]", pos0) + 1;

        // if there are more commits, get those too
        // -1 because there is always a space
        pos1 = PSF_findHash(line, pos0) - 1;

        if (pos1 >= 0)
            s = line.substr(pos0, pos1 - pos0);
        else
            s = line.substr(pos0);

        // check for strange duplicate commit names
        if (s.substr(0, s.size()/2) == s.substr(s.size()/2 + 1))
            s = s.substr(0, s.size()/2);

        v.push_back(s);

        pos0 = pos1;
    }

    return v;
}

PSF_Bug PSF_parseLineAsBug(const string &line)
{
    return PSF_Bug(PSF_getBugName(line), PSF_getBugFixes(line));
}

vector<string> parse_syzbot_fixes(const string &filename, const string &bugname)
{
    int pos0;
    ifstream inf;
    string line;
    PSF_Bug this_bug;
    vector<PSF_Fix> fixes;
    vector<string> dups;

    cout << "Parsing file: " << filename << ".\n";

    inf.open(filename);
    if (!inf)
    {
        cerr << "Error: Failed to open file " << filename << ".\n";
        return vector<string>();
    }

    while (getline(inf, line))
    {
        PSF_Bug b(PSF_parseLineAsBug(line));
        if (b.name == bugname)
        {
            this_bug.name = b.name;
            this_bug.fixes = b.fixes;
        }

        for (string f : b.fixes)
        {
            pos0 = PSF_find(f, fixes);
            if (pos0 == -1)
            {
                fixes.push_back(PSF_Fix(f));
                fixes.at(fixes.size() - 1).bugsFixed.push_back(b.name);
            }
            else
                fixes.at(pos0).bugsFixed.push_back(b.name);
        }
    }

    inf.close();

    // Gather all of the duplicates for this bug.
    for (string f : this_bug.fixes)
    {
        pos0 = PSF_find(f, fixes);
        for (string b : fixes.at(pos0).bugsFixed)
        {
            if (!PSF_is_in(b, dups))
                dups.push_back(b);
        }
    }

    return dups;
}
