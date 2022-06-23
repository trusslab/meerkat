#ifndef SHELL_API_H
#define SHELL_API_H

#include <string>

int lynx_dump(const std::string &, const std::string &);
int sed_i(const std::string &, const std::string &);
int make();

#endif