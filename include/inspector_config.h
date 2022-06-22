#ifndef INSPECTOR_CONFIG_H
#define INSPECTOR_CONFIG_H

#include <string>

class VMConfig
{
public:
    int numVM;
    int numCPU;
    int numProcs;
};

class InspectorConfig
{
private:
    std::string home_dir;
    std::string inspector_dir;
    std::string gcc_dir;
    std::string go_dir;

    VMConfig vmd;
    VMConfig vmr;
    VMConfig vmst;

    int memory;
    int makeprocs;

public:
    InspectorConfig()
    { return; }

    void parse_config_file(const std::string &);

    std::string get_home_dir() const
    { return home_dir; }

    std::string get_inspect_dir() const
    { return inspector_dir; }

    std::string get_gcc_dir() const
    { return gcc_dir; }

    std::string get_go_dir() const
    { return go_dir; }

    VMConfig get_vmd() const
    { return vmd; }

    VMConfig get_vmr() const
    { return vmr; }

    VMConfig get_vmst() const
    { return vmst; }

    int get_mem() const
    { return memory; }

    int get_makeprocs() const
    { return makeprocs; }
};

#endif