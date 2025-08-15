#ifndef SHELL_API_H
#define SHELL_API_H

#include <string>

// returns the path environment variable
std::string get_path();

// exports an environment variable
int export_env(const std::string &);

int set_timezone(const std::string &);

std::string date(const std::string &);

// calls sed -i with a given pattern and file
int sed_i(const std::string &, const std::string &);

// calls grep to look for an expression in a file.
// Returns true if the expression is found.
bool grep_to_find(const std::string &, const std::string &);

// runs cp src dest
int copy(const std::string &, const std::string &);

// runs mv src dest
int move(const std::string &, const std::string &);

int wc_l(const std::string &);

bool which(const std::string &);

#endif
