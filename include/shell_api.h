#ifndef SHELL_API_H
#define SHELL_API_H

#include <environment.h>

#include <string>
#include <vector>

// returns the path environment variable
std::string get_path();

// exports an environment variable
int export_env(const std::string &);

int set_timezone(const std::string &);

std::string date(const std::string &);

// calls lynx and dumps the result to a dump file
int lynx_dump(const std::string &, const std::string &);

// calls sed -i with a given pattern and file
int sed_i(const std::string &, const std::string &);

// calls grep to look for an expression in a file.
// Returns true if the expression is found.
bool grep_to_find(const std::string &, const std::string &);

// runs make for the current directory
int make(unsigned int, const std::string & = "", const std::string & = "");
int make(unsigned int, const std::vector<std::string> &, const std::string & = "");

// builds syzkaller for 386 POCs using syz-env
int syz_env_cross_compile(const std::string &, const std::string & = "");

// cleans up after syz-env because sudo
int syz_env_clean(const std::string &);

// runs cp src dest
int copy(const std::string &, const std::string &);

// runs mv src dest
int move(const std::string &, const std::string &);

int wc_l(const std::string &);

#endif
