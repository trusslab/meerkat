#include <consts.h>
#include <environment.h>
#include <json.h>
#include <my_string.h>

#include <string>
#include <fstream>
#include <iostream>

int Port_Info::init(const int base, const int offset, const int r = 0)
{
    range = (r > 0 ? r : range);
    start_port = base + offset * range;
    return start_port;
}

int Port_Info::inc()
{
    port_count = (port_count + 1) % range;
    port = start_port + port_count;
    return port;
}

int Environment::parse_parameters_file(const std::string & filename)
{
    std::ifstream inf;
    inf.open(filename);
    if (!inf)
    {
        std::cerr << "Error: could not open file " << filename << std::endl;
        return -1;
    }

    std::string line;
    int pos0, pos1;
    while (std::getline(inf, line))
    {
        if (line.empty() || line.at(0) == '#' )
            continue;

        pos0 = line.find_first_of("=") + 1;
        pos1 = line.find_first_of("0123456789");
        if (line.find("inspectdir=") != std::string::npos)
        {
            home = line.substr(pos0);
            home = ends_with(home, "/") ? home : home + "/";
        }
        else if (line.find("gccdir=") != std::string::npos)
        {
            gcc_dir = line.substr(pos0);
            gcc_dir = ends_with(gcc_dir, "/") ? gcc_dir : gcc_dir + "/";
        }
        else if (line.find("godir=") != std::string::npos)
        {
            go_dir = line.substr(pos0);
            go_dir = ends_with(go_dir, "/") ? go_dir : go_dir + "/";
        }
        else if (line.find("imagedir=") != std::string::npos)
        {
            image_dir = line.substr(pos0);
            image_dir = ends_with(image_dir, "/") ? image_dir : image_dir + "/";
        }
        else if (line.find("numVMd=") != std::string::npos)
        {
            vmd.numVM = std::stoi(line.substr(pos1));
        }
        else if (line.find("numCPUd=") != std::string::npos)
        {
            vmd.numCPU = std::stoi(line.substr(pos1));
        }
        else if (line.find("numProcsd=") != std::string::npos)
        {
            vmd.numProcs = std::stoi(line.substr(pos1));
        }
        else if (line.find("numVMr=") != std::string::npos)
        {
            vmr.numVM = std::stoi(line.substr(pos1));
        }
        else if (line.find("numCPUr=") != std::string::npos)
        {
            vmr.numCPU = std::stoi(line.substr(pos1));
        }
        else if (line.find("numProcsr=") != std::string::npos)
        {
            vmr.numProcs = std::stoi(line.substr(pos1));
        }
        else if (line.find("numVMst=") != std::string::npos)
        {
            vmst.numVM = std::stoi(line.substr(pos1));
        }
        else if (line.find("numCPUst=") != std::string::npos)
        {
            vmst.numCPU = std::stoi(line.substr(pos1));
        }
        else if (line.find("numProcsst=") != std::string::npos)
        {
            vmst.numProcs = std::stoi(line.substr(pos1));
        }
        else if (line.find("mem=") != std::string::npos)
        {
            memory = std::stoi(line.substr(pos1));
        }
        else if (line.find("makeproc=") != std::string::npos)
        {
            makeprocs = std::stoi(line.substr(pos1));
        }
    }

    inf.close();
    return 0;
}

int Environment::parse_config_file(const std::string & filename)
{
    JSON json;
    if (!json.parse(filename))
    {
        std::cerr << "Error: (environment) Failed to parse json file " << filename << "\n" << std::flush;
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
        kconfig = starts_with(kconfig, "/") ? kconfig : wd + kconfig;
    }
    else
    {
        std::cerr << "Error: Kernel Config was not given (\"kernel_config\": \"/path/to/config.txt\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("reproducers") && json.is_type("reproducers", JSON_Val_string))
    {
        reprodir = json.get_string("reproducers");
        reprodir = starts_with(reprodir, "/") ? reprodir : wd + reprodir;
        reprodir = ends_with(reprodir, "/") ? reprodir : reprodir + "/";
    }
    else
    {
        std::cerr << "Error: Reproducers Directory was not given (\"reproducers\": \"/path/to/reproducers/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("bugID") && json.is_type("bugID", JSON_Val_string))
    {
        working_name = json.get_string("bugID");
    }
    else
    {
        working_name.clear();
    }

    if (json.has_name("wd") && json.is_type("wd", JSON_Val_string))
    {
        wd = json.get_string("wd");
        wd = ends_with(wd, "/") ? wd : wd + "/";
    }
    else
    {
        std::cerr << "Error: No working directory was given (\"wd\": /path/to/wd/)\n" << std::flush;
        return -1;
    }

    if (json.has_name("syzconfig") && json.is_type("syzconfig", JSON_Val_string))
    {
        syzconfig = json.get_string("syzconfig");
        syzconfig = starts_with(syzconfig, "/") ? syzconfig : wd + syzconfig;
    }
    else
    {
        syzconfig = wd + "syzkaller.cfg";
    }

    if (json.has_name("syzkaller") && json.is_type("syzkaller", JSON_Val_string))
    {
        syzdir = json.get_string("syzkaller");
        syzdir = starts_with(syzdir, "/") ? syzdir : wd + syzdir;
    }

    kerneldir = wd + "kernel/";
    syzwd = wd + "wd-kaller/";
    logdir = wd + "log/";

    return 0;
}
