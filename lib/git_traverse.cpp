#include <consts.h>
#include <git_api.h>
#include <file_api.h>
#include <version.h>
#include <bug_info.h>
#include <date.h>
#include <template_parse.h>
#include <fuzz_prep.h>
#include <shell_api.h>

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <unordered_map>

using namespace std;

vector<Version> get_kernel_versions(const Bug_Info &bug, const string &old_hash, const string &new_hash)
{
    string outfile = bug.wd + "/tmp_kernel_versions.txt";
    
    cout << "Listing...\n";
    if (git_rev_list_topo(bug.kerneldir, old_hash, new_hash, outfile) < 0)
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
    cout << "Reading...\n";
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

    vp.v.name = old_hash;
    vp.v.date = git_get_commit_date(bug.wd, bug.kerneldir, old_hash);
    kernel_versions_raw.insert(pair<string,Version_p>(vp.v.name, vp));

    inf.close();
    remove_file(outfile);

    // now draw a single line using first-parent (except for the merge including the guilty commit)
    string cur_hash = new_hash;
    cout << "Parsing...\n";
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
    for (string h : LINUX_BROKEN_VERSONS)
    {
        pos0 = get_index_by_name(kernel_versions, h);
        if (pos0 >= 0)
            kernel_versions.erase(kernel_versions.begin() + pos0);
    }

    return kernel_versions;
}

vector<Version> get_syzkaller_versions(const Bug_Info &bug)
{
    int k;
    string outfile = bug.wd + "/tmp_template_changes.txt";

    git_rev_list(bug.syzdir, OLDEST_SYZKALLER_HASH, LATEST_SYZKALLER_HASH, outfile);

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
    v.date = git_get_commit_date(bug.wd, bug.syzdir, OLDEST_SYZKALLER_HASH);
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

vector<Version> get_template_changes(const Bug_Info &bug, const Date &old_date, const Date &new_date, const vector<Version> &syz_versions)
{
    vector<Version> template_changes;
    Version save;

    // traverse going back in time
    for (Version v : syz_versions)
    {
        // skip until we get to the date range
        if (v.date > new_date)
            continue;

        // stop once we are before the date range
        if (v.date < old_date)
        {
            // keep one commit before the range
            template_changes.push_back(v);
            break;
        }

        // keep only modifications to the linux template
        // older versions used sys/ rather than sys/linux/
        if (git_check_modified_file(bug.wd, bug.syzdir, v.name, "sys/linux") ||
            (v.date < Date(2017,9,15) && git_check_modified_file(bug.wd, bug.syzdir, v.name, "sys")))
        {
            template_changes.push_back(v);
            continue;
        }
    }

    return template_changes;
}

// move non-api functions to another library
vector<Version> get_relevant_template_changes(const Bug_Info &bug, const vector<Version> &template_changes)
{
    string template1 = bug.wd + "/template1.txt";
    string template2 = bug.wd + "/template2.txt";
    string template3 = bug.wd + "/template3.txt";

    vector<Version> relevant_template_changes = {template_changes.back()};

    cout << "Fetching version " << template_changes.back().name << ".\n";
    clean_syzkaller(bug);
    git_fetch_and_checkout(bug.syzdir, SYZKALLER_REPO_REMOTE, template_changes.back().name);

    cout << "Slimming version " << template_changes.back().name << ".\n";
    string template_dir = check_file(bug.syzdir + "/sys/linux") ? bug.syzdir + "/sys/linux" : bug.syzdir + "/sys";
    slim_template(bug.allreproducer, template1, list_template_files(template_dir), template_changes.back().date < OLD_INOUT_DATE);

    int l = 0;                              // left is always 0 to start
    int r = template_changes.size() - 2;    // right is one to the left of the commit we are comparing to
    int m = (l + r) / 2;
    bool same;
    while (m > 0)
    {
        while (l <= r)
        {
            m = (l + r) / 2;

            cout << SPACER
                << "Fetching version " << template_changes.at(m).name << ".\n";
            clean_syzkaller(bug);
            git_fetch_and_checkout(bug.syzdir, SYZKALLER_REPO_REMOTE, template_changes.at(m).name);

            cout << "Slimming version " << template_changes.at(m).name << ".\n";
            template_dir = check_file(bug.syzdir + "/sys/linux") ? bug.syzdir + "/sys/linux" : bug.syzdir + "/sys";
            slim_template(bug.allreproducer, template2, list_template_files(template_dir), template_changes.at(m).date < OLD_INOUT_DATE);

            same = compare_templates(template1, template2);
            if (same)
            {
                cout << "Moving left.\n";
                r = m - 1;
            }
            else
            {
                cout << "Moving right.\n";
                move(template2, template3);
                l = m + 1;
            }
        }
        
        // if the result has 2 same templates, the answer is either the commit before, or no more changes
        if (same && m > 0)
        {
            relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(m - 1));
            l = 0;
            r = m > 2 ? m - 2: 0;
        }
        else if (!same)
        {
            relevant_template_changes.insert(relevant_template_changes.begin(), template_changes.at(m));
            l = 0;
            r = m > 1 ? m - 1: 0;
        }
        else if (same)
        {
            break;
        }

        cout << "Found a relevant template change.\n";
        move(template3, template1);
    }
    remove_file(template1);
    if (check_file(template2))
        remove_file(template2);
    if (check_file(template3))
        remove_file(template3);
    
    return relevant_template_changes;
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
