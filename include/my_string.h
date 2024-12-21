#ifndef MY_STRING_H
#define MY_STRING_H

#include <string>
#include <vector>

// modifiers
std::string to_lower(std::string);
std::vector<std::string> split(std::string, char);

// checks
bool starts_with(const std::string &, const std::string &);
bool ends_with(const std::string &, const std::string &);

std::string chomp(const std::string &);
bool is_hash(const std::string &);

#endif
