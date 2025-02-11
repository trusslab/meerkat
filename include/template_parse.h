#ifndef TEMPLATE_PARSE_H
#define TEMPLATE_PARSE_H

#include <environment.h>

#include <string>
#include <vector>

// Takes in a reproducer and all of the template .txt files.
std::vector<std::string> slim_template(const std::string &, const std::vector<std::string> &);

// creates a vector with a list of all the .txt files in the given template.
std::vector<std::string> list_template_files(const std::string &, bool);

std::vector<std::string> get_reproduer_syscall_descriptions(const Environment &);

#endif
