#include <fuzz_prep.h>
#include <inspector_config.h>
#include <bug_info.h>
#include <shell_api.h>
#include <file_api.h>
#include <consts.h>
#include <date.h>
#include <version.h>
#include <template_parse.h>
#include <exec_api.h>

#include <string>
#include <fstream>
#include <iostream>

#include <string.h>
#include <unistd.h>

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

vector<Version> grab_gcc_versions(const string &filename)
{
    ifstream inf;
    inf.open(filename);
    if (!inf)
    {
        cerr << "Error: Failed to open file " << filename << ".\n";
        return vector<Version>();
    }

    vector<Version> versions;
    string line;
    int pos0;
    Version v;
    while (getline(inf, line))
    {
        if (line.at(0) == '#')
            continue;
        
        pos0 = line.find_first_of(",");
        v.date = Date(line.substr(0, pos0));
        v.name = line.substr(pos0 + 1);
        versions.push_back(v);
    }

    inf.close();
    return versions;
}

void reset_inspector()
{}

int export_gcc(const vector<Version> &versions, const Date &kernel_date, const InspectorConfig &inspector)
{
    string v;
    int i;
    // Assumes list of versions is sorted least to greatest
    for (i = versions.size() - 1; i >= 0 && kernel_date < versions.at(i).date; i--);
    i = i < 0 ? 0 : i;

    return export_env("PATH=" + inspector.get_gcc_dir() + "/" + versions.at(i).name + "/bin:" + get_path());
}

int clean_gcc(const string &old_path)
{
    return export_env("PATH=" + old_path);
}

int prep_kernel(const Bug_Info &bug, const InspectorConfig &inspector)
{
    // downloads the kernel version (does not decide)
    /*
    git fetch kernel_repo kernel_hash
    git checkout -f FETCH_HEAD
    */

    // copy over the config
    copy(bug.get_kconfig(), bug.get_kerneldir() + "/.config");

    // Handle Patches
    // apply patch from 760f8522ce08
    if (grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/scripts/selinux/mdp/mdp.c") &&
        grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/scripts/selinux/genheaders/genheaders.c") &&
        grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/security/selinux/include/classmap.h"))
    {
        cout << "Applying a patch to the kernel.\n";
        sed_i("s/#include <sys\\/socket.h>//", bug.get_kerneldir() + "/scripts/selinux/mdp/mdp.c");
        sed_i("s/#include <sys\\/socket.h>//", bug.get_kerneldir() + "/scripts/selinux/genheaders/genheaders.c");
        sed_i("s/#include <linux\\/capability.h>/#include <linux\\/capability.h>\n#include <linux\\/socket.h>/", bug.get_kerneldir() + "/security/selinux/include/classmap.h");
    }

    if (grep_to_find("ifdef CONFIG_X86_64", bug.get_kerneldir() + "/arch/x86/Makefile"))
    {
        cout << "Applying a patch to the kernel.\n";
        sed_i("/LDFLAGS :=/r patches/patch.txt", bug.get_kerneldir() + "/arch/x86/Makefile");
    }

    // build the kernel
    cd(bug.get_kerneldir());
    make(inspector.get_makeprocs(), "olddefconfig");
    cout << SPACER;
    make(inspector.get_makeprocs());
    cd(inspector.get_inspect_dir());
    return 0;
}

int prep_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector)
{
    // these will become passed args
    Date syzkaller_date(2022,6,24);
    bool slim = true;

    // work around time period where go mod tidy doesn't work
    bool dangerzone = false;
    if (syzkaller_date < Date(2020,7,4) && syzkaller_date >= Date(2020,4,30))
    {
        /*
        git fetch https://github.com/google/syzkaller 136082ab38d86932bc3ed0087694e99d0e55491b
        git checkout -f FETCH_HEAD
        */
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
        dangerzone = true;
    }

    // download syzkaller (does not decide)
    /*
    git fetch https://github.com/google/syzkaller $syzVersion
    git checkout -f FETCH_HEAD
    */

    // Slim the template
    if (slim)
    {
        cout << "Slimming the template.\n";
        string full_template = bug.get_syzdir() + "/sys/linux";
        string new_template = bug.get_wd() + "/template.txt";
        vector<string> template_files = list_template_files(full_template);
        slim_template(bug.get_repro(), new_template, template_files);
        remove_template_files(template_files);
        move(new_template, full_template);
        cout << SPACER;
    }

    // Remove the flags that check for unused functions
    sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m64 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
            bug.get_syzdir() + "/Makefile");

    // Patch a boot error related to kvm
    if (syzkaller_date < Date(2021,1,1) && syzkaller_date >= Date(2020,5,1))
    {
        cout << "Applying a patch to Syzkaller.\n";
        sed_i("s/\\-enable\\-kvm \\-cpu host,migratable=off/\\-enable\\-kvm \\-cpu host/", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }

    // Apply patch for netfilter_bridge/ebtables
    if (syzkaller_date < Date(2018,9,27) && syzkaller_date >= Date(2018,2,17))
    {
        cout << "Applying a patch to Syzkaller.\n";
        sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-1.txt", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/if.h>//", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <errno.h>/#include <errno.h>\n#include <linux\\/if.h>/", bug.get_syzdir() + "/executor/common_linux.h");
        if (check_file(bug.get_syzdir() + "/pkg/csource/generated.go"))
        {
            sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-2.txt", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/if.h>//", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <errno.h>/#include <errno.h>\n#include <linux\\/if.h>/", bug.get_syzdir() + "/pkg/csource/generated.go");
        }
    }

    // Fix a build error with strncpy
    if (syzkaller_date < Date(2018,5,13) && syzkaller_date >= Date(2018,2,10))
    {
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/pkg/csource/linux_common.go");
    }

    // Handle old go mod
    if (check_file(bug.get_syzdir() + "Godeps/Godeps.json") && !dangerzone)
    {
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
    }

    // Fix issue with earlier versions of syzkaller
    if (check_file(bug.get_syzdir() + "/vendor/cloud.google.com/go/storage/not_go110.go"))
    {
        remove_file(bug.get_syzdir() + "/vendor/cloud.google.com/go/storage/not_go110.go");
    }

    // manually generate the template if needed
    if (grep_to_find("descriptions:", bug.get_syzdir() + "/Makefile"))
    {
        cd(bug.get_syzdir());
        make(inspector.get_makeprocs(), "bin/syz-sysgen");
        char command[] = "./bin/syz-sysgen";
        char * arg_list[] = {command, nullptr};
        exec_and_wait("./bin/syz-sysgen", arg_list);
        cd(inspector.get_inspect_dir());
    }

    // Build syzkaller
    cout << "Making Syzkaller.\n";
    cd(bug.get_syzdir());
    if (make(inspector.get_makeprocs()) != 0)
        cerr << "Error: Syzkaller failed to make.\n";
    cd(inspector.get_inspect_dir());

    cout << SPACER
        << "Inserting POC as a seed.\n";
    insert_POC_as_seed(bug);

    return 0;
}

int insert_POC_as_seed(const Bug_Info &bug)
{
    // syz-db wants a directory with the seeds
    string to_pack = bug.get_wd() + "/to_pack";
    make_dir(to_pack);
    // watch if syzkaller complains about having the raw poc (with commants)
    copy(bug.get_repro(), to_pack);

    // assumes syzkaller has already been made
    string com = bug.get_syzdir() + "/bin/syz-db";
    char * command = new char[com.size() + 1];
    strcpy(command, com.c_str());
    char arg1[] = "pack";
    char * arg2 = new char[to_pack.size() + 1];
    strcpy(arg2, to_pack.c_str());
    string corpus = bug.get_kallerwd() + "/corpus.db";
    char * arg3 = new char[corpus.size() + 1];
    strcpy(arg3, corpus.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int ret = exec_and_wait(com, arg_list);

    remove_dir(to_pack);
    delete[] command;
    delete[] arg2;
    delete[] arg3;
    return ret;
}

void clean_syzkaller()
{}

void calc_bloat()
{}