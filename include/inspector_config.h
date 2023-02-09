#ifndef INSPECTOR_CONFIG_H
#define INSPECTOR_CONFIG_H

#include <string>

// stores info about the host port for easy arithmetic
class Port_Info
{
public:
    int port;
    int port_count;
    int start_port;

    Port_Info()
    { return; }
    
    Port_Info(int p, int pc, int sp)
        : port(p), port_count(pc), start_port(sp)
    { return; }

    int init(const int, const int);
    int inc(const int);
};

// stores vm resource allocation
class VMConfig
{
public:
    int numVM;
    int numCPU;
    int numProcs;
};

// stores information from the parameters.cfg file
class InspectorConfig
{
private:
    std::string home_dir;               // the directory just outside of SyzInspector
    std::string inspector_dir;          // SyzInspector
    std::string gcc_dir;                // the directory housing all of the gcc compilers
    std::string go_dir;                 // the directory housing go

    VMConfig vmd;                       // vm resource allocations for default, race, and single-thread
    VMConfig vmr;
    VMConfig vmst;

    int memory;                         // memory per vm in MB
    int makeprocs;                      // number of threads to using while making

public:
    InspectorConfig()
    { return; }

    // parses the parameters.cfg into the data structure
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
