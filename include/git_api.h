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

// takes in a repository and a commit hash (assumed to be a merge commit)
// then returns the hashes of the commits that were included in that merge.
std::vector<std::string> git_show_commits_merged(const std::string &, const std::string &);

// Takes in a repository, and rev-list of hashes, and a hash to find.
// returns the merge commit (hash) where the hash was merged.
Version git_find_merge_commit(const std::string &, const std::vector<Version> &, const std::string &);

int git_fetch_and_checkout(const std::string &, const std::string &, const std::string &);

std::string get_commit_name(const std::string &, const std::string &);

#endif
