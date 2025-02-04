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

    kerneldir = wd + "kernel/";
    syzdir = wd + "syzkaller/";
    syzwd = wd + "wd-kaller/";
    logdir = wd + "log/";

    return 0;
}
