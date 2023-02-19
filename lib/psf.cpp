#include <psf.h>
#include <bug_info.h>
#include <inspector_config.h>
#include <shell_api.h>
#include <file_api.h>
#include <consts.h>

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
    string ret;
    // find the end of the link marker. The next char is the bug name
    int pos0 = line.find_first_of("]") + 1;

    // the first case of 2 or more spaces is the end of the bug name
    int pos1 = line.find("  ", pos0);

    ret = line.substr(pos0, pos1 - pos0);

    // cut out the dup name marker: (#)
    if (ret.size() > 0 && ret.at(ret.size() - 1) == ')')
        ret = ret.substr(0, ret.find_last_of('(') - 1);

    return ret;
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
        pos0 = line.find_first_of("]", pos0);
        if (pos0 == string::npos)
            break;
        else
            pos0++;

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

vector<string> parse_syzbot_fixes(const string &filename, const string &bugname, vector<string> &dups)
{
    int pos0;
    ifstream inf;
    string line;
    PSF_Bug this_bug;
    vector<PSF_Fix> fixes;

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

    // sometimes the name is not found. If so, push back here.
    if (!PSF_is_in(bugname, dups))
        dups.push_back(bugname);

    return dups;
}

vector<string> parse_manual_duplicates(const string &filename, const string &bugname, vector<string> &dups)
{
    ifstream inf;
    string line;
    int pos0, pos1;

    if (!check_file(filename))
    {
        cerr << "Info: Manual duplicates file " << filename << " does not exist.\n";
        return dups;
    }

    inf.open(filename);
    if (!inf)
    {
        cerr << "Warning: Failed to open file " << filename << ".\n";
        return dups;
    }

    while (getline(inf, line))
    {
        if (line.empty() || line.at(0) == '#')
            continue;

        pos0 = line.find(" ~ ");

        if (line.substr(0, pos0) == bugname)
        {
            pos0 += 3;
            pos1 = line.find(" ~ ", pos0);
            while (pos1 != string::npos)
            {
                dups.push_back(line.substr(pos0, pos1 - pos0));
                pos0 = pos1 + 3;
                pos1 = line.find(" ~ ", pos0);
            }
            dups.push_back(line.substr(pos0));
        }
    }

    inf.close();
    return dups;
}

vector<string> gather_duplicates(const Bug_Info &bug, const InspectorConfig &inspector)
{
    string tmp_snapshotfile = bug.get_wd() + "/snapshot";
    vector<string> duplicates;

    lynx_dump(SYZBOT_FIXED_LINK, tmp_snapshotfile);
    trim_syzbot_fixes(tmp_snapshotfile);
    parse_syzbot_fixes(tmp_snapshotfile, bug.get_name(), duplicates);
    parse_manual_duplicates(inspector.get_inspect_dir() + "/parameters/manual_duplicates.txt", bug.get_name(), duplicates);
    remove_file(tmp_snapshotfile);

    return duplicates;
}
