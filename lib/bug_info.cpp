#include <bug_info.h>
#include <environment.h>
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

    if (json.has_name("repository") && json.is_type("repository", JSON_Val_string))
    {
        repository = json.get_string("repository");
    }
    else
    {
        std::cerr << "Error: Kernel Repository was not given\n" << std::flush;
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
        reprodir = json.get_string("reproducers");
        reprodir = starts_with(reprodir, "/") ? reprodir : env.wd + reprodir;
        reprodir = ends_with(reprodir, "/") ? reprodir : reprodir + "/";
    }
    else
    {
        std::cerr << "Error: Reproducers Directory was not given (\"reproducers\": \"/path/to/reproducers/\")\n" << std::flush;
        return -1;
    }

    return 0;
}
