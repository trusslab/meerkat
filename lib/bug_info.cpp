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
