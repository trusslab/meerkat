#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <cctype>
using namespace std;

const bool VERB = true;

// ===========================================================================
// Classes here because I wanted this to be all one file.
class Bug
{
public:
    string name;
    vector<string> fixes;

    Bug(const string &n, const vector<string> & f)
        : name(n), fixes(f)
    { return; }
};

class Fix
{
public:
    string commit;
    vector<string> bugsFixed;

    Fix(const string & c)
        : commit(c)
    { return; }
};

// ===========================================================================

// find a fix name in a vector of fixes. -1 on fail
// will sort the array if it starts to take a while
int find(const string & s, const vector<Fix> & v)
{
    for (int i = 0; i < v.size(); i++)
        if (v.at(i).commit == s)
            return i;

    return -1;
}

void log(string s)
{
    if (VERB)
        cout << s;
}

// returns the bug name from the raw text displayed on syzbot's website
// "  [##]BUGNAME      C      Done   ..."
string getBugName(const string & line)
{
    // find the end of the link marker. The next char is the bug name
    int pos0 = line.find_first_of("]") + 1;

    // the first case of 2 or more spaces is the end of the bug name
    int pos1 = line.find("  ", pos0);

    return line.substr(pos0, pos1 - pos0);
}

// returns true if the character is a hex character (a-f, 0-9)
bool ishex(char a)
{
    a = tolower(a);
    return (a >= 'a' && a <= 'f') || (a >= '0' && a <= '9');
}

// returns the location of the first 12 char hash in a given string, starting at the given location, -1 on fail
int findHash(const string & s, int i = 0)
{
    int length = 0, pos0 = 0;
    for (; i < s.size() && length < 12; i++)
    {
        if (ishex(s.at(i)))
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
vector<string> getBugFixes(const string & line)
{
    int pos0, pos1;
    string s;
    vector<string> v;

    pos0 = findHash(line);

    while (pos0 >= 0)
    {
        // find the start of the commit name
        pos0 = line.find_first_of("]", pos0) + 1;

        // if there are more commits, get those too
        // -1 because there is always a space
        pos1 = findHash(line, pos0) - 1;

        if (pos1 != -1)
            s = line.substr(pos0, pos1 - pos0);
        else
            s = line.substr(pos0);

        // check for strange duplicate commit names
        if (s.substr(0, s.size()/2) == s.substr(s.size()/2 + 1))
        {
            log("Duplicate commit name found! " + s + "\n");
            s = s.substr(0, s.size()/2);
        }

        v.push_back(s);

        pos0 = pos1;
    }

    return v;
}

Bug parseLineAsBug(const string & line)
{
    return Bug(getBugName(line), getBugFixes(line));
}

int main(int argc, char** argv)
{
    if (argc > 3)
        return 1;

    int pos0;
    ifstream inf;
    ofstream outf;
    string line;
    vector<Bug> bugs;
    vector<Fix> fixes;

    string infilename(argv[1]);
    string outfilename(argv[2]);

    log("Parsing file: " + infilename + "\n");

    inf.open(infilename);
    if (!inf)
    {
        log("Error: File failed to open!\n");
        return 1;
    }

    while (getline(inf, line))
    {
        Bug b(parseLineAsBug(line));

        for (string f : b.fixes)
        {
            pos0 = find(f, fixes);
            if (pos0 == -1)
            {
                fixes.push_back(Fix(f));
                fixes.at(fixes.size() - 1).bugsFixed.push_back(b.name);
            }
            else
                fixes.at(pos0).bugsFixed.push_back(b.name);
        }
    }

    inf.close();

    log("Output to file: " + outfilename + "\n");
    outf.open(outfilename);
    if (!outf)
    {
        log("Error: File failed to open!\n");
        return 1;
    }

    // print to file
    for (Fix f : fixes)
    {
        outf << f.commit;
        for (string s : f.bugsFixed)
            outf << " ~ " << s;
        outf << endl;
    }

    outf.close();

    return 0;
}
