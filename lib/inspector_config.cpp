#include <inspector_config.h>
#include <consts.h>

#include <string>
#include <iostream>
#include <fstream>

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

void InspectorConfig::parse_config_file(const std::string & filename)
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
        if (line.empty() || line.at(0) == '#' )
            continue;

        pos0 = line.find_first_of("=") + 1;
        pos1 = line.find_first_of("0123456789");
        if (line.find("home=") != std::string::npos)
        {
            home_dir = line.substr(pos0);
        }
        else if (line.find("inspectdir=") != std::string::npos)
        {
            inspector_dir = line.substr(pos0);
        }
        else if (line.find("gccdir=") != std::string::npos)
        {
            gcc_dir = line.substr(pos0);
        }
        else if (line.find("godir=") != std::string::npos)
        {
            go_dir = line.substr(pos0);
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
    return;
}
