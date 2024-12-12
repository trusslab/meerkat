#include <bug_info.h>
#include <environment.h>
#include <fuzz_prep.h>
#include <json.h>
#include <my_string.h>

#include <string>
#include <fstream>
#include <iostream>

int Bug_Info::parse_config_file(const Environment &env, const std::string & filename)
{
    JSON json;
    if (!json.parse(filename))
    {
        std::cerr << "Error: (bug_info) Failed to parse json file " << filename << "\n" << std::flush;
        return -1;
    }
    
    if (json.has_name("bugID") && json.is_type("bugID", JSON_Val_string))
    {
        int pos0, pos1;
        numName = json.get_string("bugID");
        pos0 = numName.find_first_of("0123456789");
        pos1 = numName.find_last_of("0123456789");
        number = std::stoi(numName.substr(pos0, pos1 - pos0 + 1));
    }
    else
    {
        std::cerr << "Error: Bug ID was not given (\"bugID\": \"bug0001\")";
        return -1;
    }
    
    if (json.has_name("bug_name") && json.is_type("bug_name", JSON_Val_string))
    {
        name = json.get_string("bug_name");
    }
    else
    {
        std::cerr << "Error: Bug Title was not given (\"bug_name\": \"KASAN: use-after-free ...\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("bug_link") && json.is_type("bug_link", JSON_Val_string))
    {
        buglink = json.get_string("bug_link");
    }
    else
    {
        std::cerr << "Error: Bug Link was not given (\"bug_link\": \"https://...\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("arch") && json.is_type("arch", JSON_Val_string))
    {
        arch = json.get_string("arch");
    }
    else
    {
        std::cerr << "Error: Arch was not given (\"arch\": \"amd64|i386\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("kernel_branch") && json.is_type("kernel_branch", JSON_Val_string))
    {
        kpreface = json.get_string("kernel_branch");
    }
    else
    {
        std::cerr << "Error: Kernel branch was not given (\"kernel_branch\": \"linux\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("short_repository") && json.is_type("short_repository", JSON_Val_string))
    {
        repository = json.get_string("short_repository");
    }
    else
    {
        std::cerr << "Error: Kernel Repository was not given (\"short_repository\": \"torvalds/linux.git\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("finding_hash") && json.is_type("finding_hash", JSON_Val_string))
    {
        find_hash = json.get_string("finding_hash");
    }
    else
    {
        std::cerr << "Error: Finding Hash was not given (\"finding_hash\": \"03ad...\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("finding_date") && json.is_type("finding_date", JSON_Val_string))
    {
        find_date = Date(json.get_string("finding_date"));
        find_date.set_delim('-');
    }

    if (json.has_name("guilty_hash") && json.is_type("guilty_hash", JSON_Val_string))
    {
        guilty_hash = json.get_string("guilty_hash");
    }
    else
    {
        std::cerr << "Error: Guilty Hash was not given (\"guilty_hash\": \"03ad...\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("kernel_config") && json.is_type("kernel_config", JSON_Val_string))
    {
        kconfig = json.get_string("kernel_config");
        kconfig = starts_with(kconfig, "/") ? kconfig : env.wd + kconfig;
    }
    else
    {
        std::cerr << "Error: Kernel Config was not given (\"kernel_config\": \"/path/to/config.txt\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("reproducers") && json.is_type("reproducers", JSON_Val_string))
    {
        reproducer = json.get_string("reproducers");
        reproducer = starts_with(reproducer, "/") ? reproducer : env.wd + reproducer;
        reproducer = ends_with(reproducer, "/") ? reproducer : reproducer + "/";
    }
    else
    {
        std::cerr << "Error: Reproducers Directory was not given (\"reproducers\": \"/path/to/reproducers/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("all_reproducers") && json.is_type("all_reproducers", JSON_Val_string))
    {
        allreproducer = json.get_string("all_reproducers");
        allreproducer = starts_with(allreproducer, "/") ? allreproducer : env.wd + allreproducer;
    }
    else
    {
        std::cerr << "Error: Concatenated Reproducers were not given (\"all_reproducers\": \"/path/to/all_reproducers.prog\")\n" << std::flush;
        return -1;
    }

    return 0;
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
