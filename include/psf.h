#ifndef PSF_H
#define PSF_H

#include <environment.h>

#include <string>
#include <vector>

std::vector<std::string> parse_manual_duplicates(const std::string &, const std::string &, std::vector<std::string> &);

std::vector<std::string> gather_duplicates(const Environment &);

#endif
