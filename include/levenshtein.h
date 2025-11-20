#ifndef LEVENSHTEIN_H
#define LEVENSHTEIN_H

#include <string>
#include <vector>

// Author: JTBursey
// This is a library for computing the Levenshtein distance between two objects.
// See wiki: https://en.wikipedia.org/wiki/Levenshtein_distance

// Presents the levenshtein distance where smaller is more similar.
int levenshtein_str(const std::string &, const std::string &);
int levenshtein_vec(const std::vector<std::string> &, const std::vector<std::string> &);

// Normalizes the levenshtein distance to a similarity score in [0,1] where 0 is more similar.
// I'll let the user do the 1 - x part so this library always has 0 is better.
double levenshtein_str_norm(const std::string &, const std::string &);
double levenshtein_vec_norm(const std::vector<std::string> &, const std::vector<std::string> &);

#endif
