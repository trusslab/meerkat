#include <bug_info.h>
#include <environment.h>

#include <string>
#include <fstream>
#include <iostream>

void Environment::parse_config_file(const Bug_Info &bug, const std::string & filename)
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
        if (line.find("kallerwd=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzwd = line.substr(pos0);
        }
        else if (line.find("syzconfig=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzconfig = line.substr(pos0);
        }
        else if (line.find("managerwd=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            wd = line.substr(pos0);
        }
        else if (line.find("syzdir=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzdir = line.substr(pos0);
        }
    }

    syzkaller_log = wd + "/log/bug" + std::to_string(bug.number) + "-kaller.log";
    kerneldir = wd + "/kernel";

    inf.close();
    return;
}
