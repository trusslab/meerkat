#ifndef MAKE_H
#define MAKE_H

#include <string>
#include <vector>

// runs make for the current directory
int make(unsigned int, const std::string & = "", const std::string & = "");
int make(unsigned int, const std::vector<std::string> &, const std::string & = "");

#endif
