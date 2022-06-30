#ifndef GIT_API_H
#define GIT_API_H

#include <version.h>
#include <bug_info.h>
#include <date.h>

#include <string>
#include <vector>

void git_init(const std::string &);

int set_timezone(const std::string &);

std::vector<Version> get_kernel_versions(const Bug_Info &, const std::string &, const std::string &);
std::vector<Version> get_syzkaller_versions(const Bug_Info &bug);
std::vector<Version> get_template_changes(const Bug_Info &, const Date &, const Date &, const std::vector<Version> &);

int git_fetch_and_checkout(const std::string &, const std::string &, const std::string &);

#endif
