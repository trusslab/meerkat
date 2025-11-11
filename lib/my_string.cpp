#include <my_string.h>

#include <string>
#include <cctype>

// Returns the lower case of the given string.
std::string to_lower(std::string str)
{
    for (int i = 0; i < str.size(); i++)
        str.at(i) = tolower(str.at(i));
    return str;
}

// splits the string "str" based on delimiter "d". Returns the resulting strings.
std::vector<std::string> split(std::string str, char d)
{
    std::vector<std::string> split;
    int pos;

    if (str.empty())
        return {};

    do {
        pos = str.find_first_of(d);
        if (pos != 0)
            split.push_back(str.substr(0, pos));
        str = str.substr(pos+1);
    } while (pos != std::string::npos && !str.empty());

    return split;
}

// Returns true if "str" begins with "prefix", false otherwise.
bool starts_with(const std::string &str, const std::string &prefix)
{
    int i = 0;
    for (; i < str.size() && i < prefix.size(); i++)
    {
        if (str.at(i) != prefix.at(i))
            return false;
    }
    return i >= prefix.size();
}

bool ends_with(const std::string &str, const std::string &postfix)
{
    int i = str.size() - 1, j = postfix.size() - 1;
    for (; i >= 0 && j >= 0; i--, j--)
    {
        if (str.at(i) != postfix.at(j))
            return false;
    }
    return j < 0;
}

std::string chomp(const std::string &str)
{
    // Remove return carriages as well.
    std::string ret = ends_with(str, "\n") ? str.substr(0, str.size() - 1) : str;
    ret = ends_with(str, "\r") ? str.substr(0, str.size() - 1) : str;
    return ret;
}

// Removes spaces from the front of a string (using isspace from cctype)
std::string trim_space(std::string str)
{
    int pos = 0;
    for (pos = 0; pos < str.size() && isspace(str.at(pos)); pos++);
    return str.substr(pos);
}

bool is_hash(const std::string &str)
{
    bool ishash = true;
    for (char a : str)
        if (!isxdigit(a))
            return false;
    
    return true;
}
