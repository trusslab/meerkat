#ifndef GIT_TRAVERSE
#define GIT_TRAVERSE

#include <bug_info.h>
#include <date.h>
#include <environment.h>
#include <version.h>

#include <string>
#include <vector>

std::vector<Version> get_kernel_versions(const Environment &, const Bug_Info &);
std::vector<Version> get_syzkaller_versions(const Environment &);

// Takes in a repository, and rev-list of hashes, and a hash to find.
// returns the merge commit (hash) where the hash was merged.
Version git_find_merge_commit(const std::string &, const std::vector<Version> &, const std::string &);

#endif