#include <vector>
#include <string>

using namespace std;

// Quick and dirty search function for string vectors
bool fuzz_is_in(const string &s, const vector<string> &v)
{
    for (string str : v)
        if (str == s)
            return true;
    
    return false;
}
