#include <fuzz_prep.h>
#include <inspector_config.h>
#include <bug_info.h>

#include <string>
#include <fstream>
#include <iostream>

using namespace std;

int get_procs_from_repro(const string & repro)
{
    int p = -1, pos0;
    string line;
    ifstream inf;
    inf.open(repro);
    if (!inf)
    {
        cerr << "Error: Failed to open file " << repro << ".\n";
        return -1;
    }

    while (getline(inf, line))
    {
        pos0 = line.find("\"procs\":");
        if (pos0 != string::npos)
        {
            pos0 += 8;
            p = line.at(pos0) - '0';
            break;
        }
    }

    inf.close();
    return p;
}

VMConfig determine_threadedness(const InspectorConfig &inspector, const Bug_Info &bug, std::ostream &logfile)
{
    int procs = get_procs_from_repro(bug.get_repro());
    VMConfig vmc;
    switch (procs)
    {
    case 1:
        cout << "Using resource allocation for a single threaded bug.\n";
        logfile << "Single Threaded Allocation\n";
        vmc = inspector.get_vmst();
        break;
    case 6:
        cout << "Using default resource allocation.\n";
        logfile << "Default Allocation.\n";
        vmc = inspector.get_vmd();
        break;
    case 8:
        cout << "Using resource allocation for a race bug.\n";
        logfile << "Race Allocation.\n";
        vmc = inspector.get_vmr();
        break;
    default:
        cerr << "Warning: Could not retrieve number of procs from reproducer " << bug.get_repro() << ". Using Default.\n";
        vmc = inspector.get_vmd();
    }
    logfile << "VMs:" << vmc.numVM << endl
            << "CPUs:" << vmc.numCPU << endl
            << "Procs:" << vmc.numProcs << endl;

    return vmc;
}