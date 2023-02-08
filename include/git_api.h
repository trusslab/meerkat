#ifndef GIT_API_H
#define GIT_API_H

#include <version.h>
#include <bug_info.h>
#include <date.h>

#include <string>
#include <vector>

void git_init(const std::string &);

Date git_get_commit_date(const std::string &, const std::string &, const std::string &);

bool git_check_modified_file(const std::string &, const std::string &, const std::string &, const std::string &);

int git_rev_list(const std::string &, const std::string &, const std::string &, const std::string &);
int git_rev_list_topo(const std::string &, const std::string &, const std::string &, const std::string &);

// takes in a repository and a commit hash (assumed to be a merge commit)
// then returns the hashes of the commits that were included in that merge.
std::vector<std::string> git_show_commits_merged(const std::string &, const std::string &);

int git_clone(const string &, const string &);
int git_fetch_and_checkout(const std::string &, const std::string &, const std::string &);

std::string get_commit_name(const std::string &, const std::string &);

#endif
