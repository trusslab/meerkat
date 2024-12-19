#ifndef GIT_TRAVERSE
#define GIT_TRAVERSE

#include <date.h>
#include <environment.h>
#include <git.h>
#include <version.h>

#include <string>
#include <vector>

std::vector<Version> get_kernel_versions(const Environment &, Git &, const std::string &, const std::string &);
std::vector<Version> get_syzkaller_versions(const Environment &, Git &, const std::string &, const std::string &);

#endif