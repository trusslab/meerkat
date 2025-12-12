#include <levenshtein.h>

#include <string>
#include <vector>
#include <map>

int min3(unsigned int x, unsigned int y, unsigned int z)
{
    return x < y ? (x < z ? x : z) : (y < z ? y : z);
}

// That feeling when it's easier to write the function yourself than include the library.
int max2(unsigned int x, unsigned int y)
{
    return x > y ? x : y;
}

int levenshtein_str(const std::string &s1, const std::string &s2)
{
    if (s1.empty())
        return s2.size();
    if (s2.empty())
        return s1.size();

    if (s1.front() == s2.front())
        return levenshtein_str(s1.substr(1), s2.substr(1));

    return 1 + min3(levenshtein_str(s1.substr(1), s2.substr(1)),
                    levenshtein_str(s1, s2.substr(1)),
                    levenshtein_str(s1.substr(1), s2));
}

// Levenshtein implementation using indices to minimize memory copies.
// This could be templated, but that would be even harder than duplicating the code.
int _levenshtein_vec(const std::vector<std::string> &v1, unsigned int i1, const std::vector<std::string> &v2, unsigned int i2, std::map<std::pair<unsigned int, unsigned int>, int> &mem)
{
    if (mem.count({i1, i2}) > 0)
        goto out;

    if (i1 >= v1.size())
        mem.insert({{i1, i2}, v2.size() - i2});
    else if (i2 >= v2.size())
        mem.insert({{i1, i2}, v1.size() - i1});
    else if (v1.at(i1) == v2.at(i2))
        mem.insert({{i1, i2}, _levenshtein_vec(v1, i1 + 1, v2, i2 + 1, mem)});
    else
        mem.insert({{i1, i2}, 1 + min3(_levenshtein_vec(v1, i1 + 1, v2, i2 + 1, mem),
                                       _levenshtein_vec(v1, i1,     v2, i2 + 1, mem),
                                       _levenshtein_vec(v1, i1 + 1, v2, i2, mem))});
out:
    return mem.at({i1, i2});
}

int levenshtein_vec(const std::vector<std::string> &v1, const std::vector<std::string> &v2)
{
    std::map<std::pair<unsigned int, unsigned int>, int> mem;
    return _levenshtein_vec(v1, 0, v2, 0, mem);
}

double levenshtein_str_norm(const std::string &s1, const std::string &s2)
{
    if (s1.empty() && s2.empty())
        return 0;
    return static_cast<double>(levenshtein_str(s1, s2)) / static_cast<double>(max2(s1.size(), s2.size()));
}

double levenshtein_vec_norm(const std::vector<std::string> &v1, const std::vector<std::string> &v2)
{
    if (v1.empty() && v2.empty())
        return 0;
    return static_cast<double>(levenshtein_vec(v1, v2)) / static_cast<double>(max2(v1.size(), v2.size()));
}
