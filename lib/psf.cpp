#include <environment.h>
#include <psf.h>
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

vector<string> gather_duplicates(Environment &env)
{
    vector<string> duplicates;
    parse_manual_duplicates(env.home + "/parameters/manual_duplicates.txt", env.name, env.duplicates);
    return duplicates;
}
