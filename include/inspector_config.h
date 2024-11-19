#ifndef INSPECTOR_CONFIG_H
#define INSPECTOR_CONFIG_H

#include <string>

enum Compiler_Setting {COMPILER_GCC = 0, COMPILER_CLANG, COMPILER_CLANG_14};

// stores info about the host port for easy arithmetic
// The port must change in between runs of Syzkaller in
// case the previous port was not freed yet.
class Port_Info
{
public:
    int port;
    int port_count;
    int start_port;
    int range;

    Port_Info()
        : port(0), port_count(0), start_port(0), range(3)
    { return; }
    
    Port_Info(int p, int pc, int sp, int r)
        : port(p), port_count(pc), start_port(sp), range(r)
    { return; }

    int init(const int, const int, const int);
    int inc();
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
    std::string image_dir;              // directory of the os images

    VMConfig vmd;                       // vm resource allocations for default, race, and single-thread
    VMConfig vmr;
    VMConfig vmst;

    int memory;                         // memory per vm in MB
    int makeprocs;                      // number of threads to using while making

public:
    Compiler_Setting compiler_setting;
    VMConfig vmc;

    Port_Info port;

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

    std::string get_image_dir() const
    { return image_dir; }

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
