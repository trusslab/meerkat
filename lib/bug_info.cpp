#include <bug_info.h>

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
            kernel_config = line.substr(pos0);
        }
        else if (line.find("repro=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            reproducer = line.substr(pos0);
        }
        else if (line.find("kallerwd=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzkaller_wd = line.substr(pos0);
        }
        else if (line.find("syzconfig=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzkaller_config = line.substr(pos0);
        }
        else if (line.find("managerwd=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            wd = line.substr(pos0);
        }
        else if (line.find("syzdir=") != std::string::npos)
        {
            pos0 = line.find_first_of("=") + 1;
            syzkaller_dir = line.substr(pos0);
        }
        else if (line.find("buglink=") != std::string::npos)
        {
            pos0 = line.find_first_of("\"") + 1;
            pos1 = line.find_last_of("\"");
            bug_link = line.substr(pos0, pos1 - pos0);
        }
    }

    syzkaller_log = wd + "/log/bug" + std::to_string(number) + "-kaller.log";
    kernel_dir = wd + "/kernel";

    inf.close();
    return;
}