#ifndef GIT_TRAVERSE
#define GIT_TRAVERSE

#include <version.h>
#include <bug_info.h>
#include <date.h>

#include <string>
#include <vector>

std::vector<Version> get_kernel_versions(const Bug_Info &, const std::string &, const std::string &);
std::vector<Version> get_syzkaller_versions(const Bug_Info &bug);
std::vector<Version> get_template_changes(const Bug_Info &, const Date &, const Date &, const std::vector<Version> &);
std::vector<Version> get_relevant_template_changes(const Bug_Info &, const std::vector<Version> &);

// Takes in a repository, and rev-list of hashes, and a hash to find.
// returns the merge commit (hash) where the hash was merged.
Version git_find_merge_commit(const std::string &, const std::vector<Version> &, const std::string &);

#endif