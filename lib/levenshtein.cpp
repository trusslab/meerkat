#include <levenshtein.h>

#include <string>
#include <vector>

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
int _levenshtein_vec(const std::vector<std::string> &v1, unsigned int i1, const std::vector<std::string> &v2, unsigned int i2)
{
    if (i1 >= v1.size())
        return v2.size();
    if (i2 >= v2.size())
        return v1.size();

    if (v1.at(i1) == v2.at(i2))
        return _levenshtein_vec(v1, i1 + 1, v2, i2 + 1);

    return 1 + min3(_levenshtein_vec(v1, i1 + 1, v2, i2 + 1),
                    _levenshtein_vec(v1, i1, v2, i2 + 1),
                    _levenshtein_vec(v1, i1 + 1, v2, i2));
}

int levenshtein_vec(const std::vector<std::string> &v1, const std::vector<std::string> &v2)
{
    return _levenshtein_vec(v1, 0, v2, 0);
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
