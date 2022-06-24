#ifndef FILE_API_H
#define FILE_API_H

#include <string>
#include <vector>

// Checks if a file exists
bool check_file(const std::string &);

// makes a directory with the given name
int make_dir(const std::string &);

// removes a file or empty directory
int remove_file(const std::string &);

// removes a full directory
int remove_dir(const std::string &);

// cd
int cd(const std::string &);

// lists the contents of the given dir. Returns
// a list of filenames in a vector.
std::vector<std::string> list_dir(const std::string &);

#endif