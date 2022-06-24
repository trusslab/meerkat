#ifndef FILE_API_H
#define FILE_API_H

#include <string>
#include <vector>

bool check_file(const std::string &);
int make_dir(const std::string &);
int remove_file(const std::string &);
int remove_dir(const std::string &);
int cd(const std::string &);
std::vector<std::string> list_dir(const std::string &);

#endif