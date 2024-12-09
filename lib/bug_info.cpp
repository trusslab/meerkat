#include <bug_info.h>
#include <fuzz_prep.h>
#include <my_string.h>

#include <string>
#include <fstream>
#include <iostream>

void Bug_Info::parse_config_file(const std::string & filename)
{
    std::ifstream inf;
    inf.open(filename);
    if (!inf)
    {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return;
    }

    std::string line;
    int pos0, pos1;
    while (std::getline(inf, line))
    {
        if (line.find("curBug=") != std::string::npos)
        {
            pos0 = line.find_first_of("0123456789");
            pos1 = line.find_last_of("0123456789");
            number = std::stoi(line.substr(pos0, pos1 - pos0 + 1));
            pos0 = line.find_first_of("\"") + 1;
            pos1 = line.find_last_of("\"");
            numName = line.substr(pos0, pos1 - pos0);
        }
        else if (line.find("bugname=") != std::string::npos)
        {
            pos0 = line.find_first_of("\"") + 1;
            pos1 = line.find_last_of("\"");
            name = line.substr(pos0, pos1 - pos0);
        }
        else if (line.find("kpref=") != std::string::npos)
        {
            pos0 = line.find_first_of("\"") + 1;
            pos1 = line.find_last_of("\"");
            kpreface = line.substr(pos0, pos1 - pos0);
        }
        else if (line.find("repo=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            repository = line.substr(pos0);
        }
        else if (line.find("kconfig=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            kconfig = line.substr(pos0);
        }
        else if (line.find("repro=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            reproducer = line.substr(pos0);
        }
        else if (line.find("reproall=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            allreproducer = line.substr(pos0);
        }
        else if (line.find("buglink=") != std::string::npos)
        {
            pos0 = line.find_first_of("\"") + 1;
            pos1 = line.find_last_of("\"");
            buglink = line.substr(pos0, pos1 - pos0);
        }
        else if (line.find("arch=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            arch = line.substr(pos0);
        }
    }

    inf.close();
    return;
}

// Tries to remove the config related to the sanitizer finding the blocking bug
int remove_related_config(const Bug_Info &bug, const std::string &bb)
{
    int err = 0;
    std::string sanitizer = split(bb, ' ').front();
    // If any duplicates need the sanitizer, don't remove it
    for (std::string dup : bug.duplicates)
        if (sanitizer.find(split(dup, ' ').front()) != std::string::npos)
            return 0;

    if (sanitizer.find("UBSAN") != std::string::npos)
    {
        err = unset_kernel_config(bug.kconfig, {"CONFIG_UBSAN", "CONFIG_UBSAN_TRAP", "CONFIG_UBSAN_BOUNDS", "CONFIG_UBSAN_SHIFT"});
        if (err >= 0)
            return 1;
    }
    return 0;
}

// Tries to patch the given blocking bug
int attempt_patch(const Bug_Info &bug, const std::string &bb)
{
    // TODO: find a backport for this bug
    // try to remove a config related to the bug
    if (remove_related_config(bug, bb) > 0)
        return 1;
        
    return 0;
}

// Identifies blocking bugs from the result, then figures if they can be removed.
// Returns the number of removed blocking bugs or -1 on error
int patch_blocking_bugs(const Test_Result &result, const Bug_Info &bug)
{
    int count = 0;
    if (result.found)
        return 0;
    
    std::vector<std::string> bbs = get_prominent_blocking_bugs(result);
    for (std::string b : bbs)
        count += attempt_patch(bug, b);
    return count;
}
