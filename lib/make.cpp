#include <make.h>
#include <my_string.h>
#include <exec_api.h>

#include <iostream>
#include <string>
#include <vector>

int make(unsigned int procs, const std::vector<std::string> &opts, const std::string &outfile)
{
    std::string argstr = "make -j" + std::to_string(procs) + " -f Makefile";
    std::vector<std::string> spl = split(argstr, ' ');

    for (std::string o : opts)
        if (!o.empty())
            spl.push_back(o);
    
    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = exec_and_wait("make", (char **)arg_list, outfile, outfile);
    if (err != 0)
        std::cerr << "Warning: make exited with error status 0x" << std::hex << err << std::endl << std::dec << std::flush;

    delete[] arg_list;
    return (err == 0 ? 0 : -1);
}

int make(unsigned int procs, const std::string &option, const std::string &outfile)
{
    std::string argstr = "make -j" + std::to_string(procs) + " -f Makefile";
    std::vector<std::string> spl = split(argstr, ' ');

    if (!option.empty())
        spl.push_back(option);

    const char ** arg_list = new const char*[spl.size()+1];
    for (int i = 0; i < spl.size(); i++)
        arg_list[i] = spl.at(i).c_str();

    arg_list[spl.size()] = nullptr;

    int err = exec_and_wait("make", (char **)arg_list, outfile, outfile);
    if (err != 0)
        std::cerr << "Warning: make exited with error status 0x" << std::hex << err << std::endl << std::dec << std::flush;

    delete[] arg_list;
    return (err == 0 ? 0 : -1);
}
