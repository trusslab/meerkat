#include <bug_info.h>
#include <consts.h>
#include <date.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz_prep.h>
#include <git_api.h>
#include <shell_api.h>
#include <template_parse.h>
#include <version.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace std;

vector<Version> get_kernel_versions(const Environment &env, const Bug_Info &bug)
{
    string outfile = env.wd + "/tmp_kernel_versions.txt";

    if (git_rev_list_topo(env.kerneldir, bug.guilty_hash, bug.find_hash, outfile) < 0)
    {
        return vector<Version>();
    }

    ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n" << flush;
        return vector<Version>();
    }

    string line;
    unordered_map<string, Version_p> kernel_versions_raw;
    vector<Version> kernel_versions;
    Version_p vp;
    int pos0 = 0, pos1 = 0, count = 0;
    while(getline(inf, line))
    {
        vp.parents.clear();
        vp.v.name = line.substr(7);
        getline(inf, line);
        pos0 = 0;
        pos1 = line.find_first_of(" ");
        vp.v.date = Date(line.substr(pos0, pos1 - pos0));

        pos0 = pos1 + 1;
        pos1 = line.find_first_of(" ", pos0);
        while (pos1 != string::npos)
        {
            vp.parents.push_back(line.substr(pos0, pos1 - pos0));
            pos0 = pos1 + 1;
            pos1 = line.find_first_of(" ", pos0);
        }
        vp.parents.push_back(line.substr(pos0));

        kernel_versions_raw.insert(pair<string,Version_p>(vp.v.name, vp));
    }

    vp.v.name = bug.guilty_hash;
    vp.v.date = git_get_commit_date(env.wd, env.kerneldir, bug.guilty_hash);
    kernel_versions_raw.insert(pair<string,Version_p>(vp.v.name, vp));

    inf.close();
    remove_file(outfile);

    // now draw a single line using first-parent (except for the merge including the guilty commit)
    string cur_hash = bug.find_hash;
    kernel_versions.push_back(kernel_versions_raw.at(cur_hash).v);
    while (kernel_versions.back().name != bug.guilty_hash)
    {
        // starting with the first parent, if it exists, push back
        for (int j = 0; j < kernel_versions_raw.at(cur_hash).parents.size(); j++)
        {
            if (kernel_versions_raw.find(kernel_versions_raw.at(cur_hash).parents.at(j)) != kernel_versions_raw.end())
            {
                kernel_versions.push_back(kernel_versions_raw.at(kernel_versions_raw.at(cur_hash).parents.at(j)).v);
                cur_hash = kernel_versions_raw.at(cur_hash).parents.at(j);
                break;
            }
        }
    }

    // remove bad versions of the kernel
    for (string h : LINUX_BROKEN_VERSONS)
    {
        pos0 = get_index_by_name(kernel_versions, h);
        if (pos0 >= 0)
            kernel_versions.erase(kernel_versions.begin() + pos0);
    }

    return kernel_versions;
}

vector<Version> get_syzkaller_versions(const Environment &env)
{
    int k;
    string outfile = env.wd + "/tmp_syz_versions.txt";

    string latest_syzkaller_hash = get_latest_commit_hash(env.syzdir);
    git_rev_list(env.syzdir, OLDEST_SYZKALLER_HASH, latest_syzkaller_hash, outfile);

    ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        cerr << "Error: Failed to open temp file " << outfile << ".\n";
        return vector<Version>();
    }

    string line;
    vector<Version> syzkaller_versions;
    Version v;
    while(getline(inf, line))
    {
        v.name = line.substr(7);
        getline(inf, line);
        v.date = Date(line);
        syzkaller_versions.push_back(v);
    }

    v.name = OLDEST_SYZKALLER_HASH;
    v.date = git_get_commit_date(env.wd, env.syzdir, OLDEST_SYZKALLER_HASH);
    syzkaller_versions.push_back(v);

    // remove bad versions of syzkaller
    for (string h : SYZKALLER_BROKEN_VERSONS)
    {
        k = get_index_by_name(syzkaller_versions, h);
        if (k >= 0)
            syzkaller_versions.erase(syzkaller_versions.begin() + k);
    }

    inf.close();
    remove_file(outfile);
    return syzkaller_versions;
}

Version git_find_merge_commit(const string &repo, const vector<Version> &commits, const string &hash_to_find)
{
    vector<string> commits_merged;
    // The merge commit SHOULD be the commit at index commits.size() - 2.
    // Certainly don't go searching up until the finding commit. Cut it at
    // 1000 commits for now. That should leave some head room in case I'm
    // wrong while still not wasting too much time.
    for (int i = commits.size() - 1, j = 0; i >= 0 && j < 1000; i--, j++)
    {
        if (commits.at(i).name == hash_to_find)
            continue;

        commits_merged.clear();
        commits_merged = git_show_commits_merged(repo, commits.at(i).name);
        for (string c : commits_merged)
            if (hash_to_find == c)
                return commits.at(i);
    }

    return Version("", Date(0,0,0));
}
