#include <consts.h>
#include <date.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz_prep.h>
#include <git.h>
#include <shell_api.h>
#include <template_parse.h>
#include <version.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

std::vector<Version> get_kernel_versions(const Environment &env, Git &linux_git, const std::string &old_hash, const std::string &new_hash)
{
    std::string outfile = env.wd + "tmp_kernel_versions.txt";

    if (linux_git.revlist_topo(old_hash, new_hash, outfile) < 0)
        return std::vector<Version>();

    std::ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        std::cerr << "Error: Failed to open temp file " << outfile << ".\n" << std::flush;
        return std::vector<Version>();
    }

    std::string line;
    std::unordered_map<std::string, Version_p> kernel_versions_raw;
    std::vector<Version> kernel_versions;
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
        while (pos1 != std::string::npos)
        {
            vp.parents.push_back(line.substr(pos0, pos1 - pos0));
            pos0 = pos1 + 1;
            pos1 = line.find_first_of(" ", pos0);
        }
        vp.parents.push_back(line.substr(pos0));

        kernel_versions_raw.insert(std::pair<std::string,Version_p>(vp.v.name, vp));
    }

    vp.v.name = old_hash;
    vp.v.date = linux_git.get_commit_date(old_hash);
    kernel_versions_raw.insert(std::pair<std::string,Version_p>(vp.v.name, vp));

    inf.close();
    remove_file(outfile);

    // now draw a single line using first-parent (except for the merge including the guilty commit)
    std::string cur_hash = new_hash;
    kernel_versions.push_back(kernel_versions_raw.at(cur_hash).v);
    while (kernel_versions.back().name != old_hash)
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
    for (std::string h : LINUX_BROKEN_VERSONS)
    {
        pos0 = get_index_by_name(kernel_versions, h);
        if (pos0 >= 0)
            kernel_versions.erase(kernel_versions.begin() + pos0);
    }

    return kernel_versions;
}

std::vector<Version> get_syzkaller_versions(const Environment &env, Git &syzkaller_git, const std::string &old_hash, const std::string &new_hash)
{
    int k;
    std::string outfile = env.wd + "tmp_syz_versions.txt";

    syzkaller_git.revlist(old_hash, new_hash, outfile);

    std::ifstream inf;
    inf.open(outfile);
    if (!inf)
    {
        std::cerr << "Error: Failed to open temp file " << outfile << ".\n" << std::flush;
        return std::vector<Version>();
    }

    std::string line;
    std::vector<Version> syzkaller_versions;
    Version v;
    while(getline(inf, line))
    {
        v.name = line.substr(7);
        getline(inf, line);
        v.date = Date(line);
        syzkaller_versions.push_back(v);
    }

    v.name = old_hash;
    v.date = syzkaller_git.get_commit_date(old_hash);
    syzkaller_versions.push_back(v);

    // remove bad versions of syzkaller
    for (std::string h : SYZKALLER_BROKEN_VERSONS)
    {
        k = get_index_by_name(syzkaller_versions, h);
        if (k >= 0)
            syzkaller_versions.erase(syzkaller_versions.begin() + k);
    }

    inf.close();
    remove_file(outfile);
    return syzkaller_versions;
}
