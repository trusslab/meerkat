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
#include <git_api.h>
#include <inspect.h>

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

vector<Version> grab_compiler_versions(const string &filename)
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

string export_compiler_sub(const vector<Version> &versions, const Date &kernel_date, const InspectorConfig &inspector)
{
    string v;
    int i;
    // Assumes list of versions is sorted least to greatest
    for (i = versions.size() - 1; i >= 0 && kernel_date < versions.at(i).date; i--);
    i = i < 0 ? 0 : i;

    export_env("PATH=" + inspector.get_gcc_dir() + "/" + versions.at(i).name + "/bin:" + get_path());
    return versions.at(i).name;
}

string export_compiler(const vector<Version> &gcc_versions, const vector<Version> &clang_versions, const Date &kernel_date, const InspectorConfig &inspector, bool useclang)
{
    if (useclang)
        return export_compiler_sub(clang_versions, kernel_date, inspector);
    else
        return export_compiler_sub(gcc_versions, kernel_date, inspector);
    
    return "";
}

int clean_path(const string &old_path)
{
    return export_env("PATH=" + old_path);
}

int prep_kernel(const Bug_Info &bug, const InspectorConfig &inspector, const Version &linux_version, const string &repo) // repoistory &repo, const hash
{
    int err = 0;

    cout << "Cleaning the kernel.\n";
    clean_kernel(bug);
    cout << SPACER;

    // downloads the kernel version (does not decide)
    err = git_fetch_and_checkout(bug.get_kerneldir(), repo, linux_version.name);
    if (err < 0)
        return err;

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
    err = make(inspector.get_makeprocs(), "olddefconfig");
    if (err < 0)
        return err;
    cout << SPACER;
    err = make(inspector.get_makeprocs());
    if (err < 0)
    {
        cerr << "Error: The kernel failed to make.\n";
        return err;
    }
    cd(inspector.get_inspect_dir());
    return err;
}

int clean_kernel(const Bug_Info &bug)
{
    string old_dir = pwd();
    int err = 0;

    cd(bug.get_kerneldir());
    err = make(1, "clean");
    cd(old_dir);

    return (err == 0 ? 0 : -1);
}

int prep_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const Version &syzkaller_version, const string &use_template)
{
    int err = 0;
    if (bug.get_arch() == "i386")
        err = syz_env_clean(inspector.get_inspect_dir() + "/tools/syz-env");
    
    err = clean_syzkaller(bug);

    if (err < 0)
        return err;

    // export targetvmarch and target arch if building for 386
    if (bug.get_arch() == "i386")
    {
        export_env("TARGETVMARCH=amd64");
        export_env("TARGETARCH=386");
    }

    // work around time period where go mod tidy doesn't work
    bool dangerzone = false;
    if (syzkaller_version.date < Date(2020,7,4) && syzkaller_version.date >= Date(2020,4,30))
    {
        err = git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, "136082ab38d86932bc3ed0087694e99d0e55491b");
        if (err < 0)
            return err;

        cd(bug.get_syzdir());
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
        cd(inspector.get_inspect_dir());
        dangerzone = true;
    }

    // download syzkaller (does not decide)
    err = git_fetch_and_checkout(bug.get_syzdir(), SYZKALLER_REPO_REMOTE, syzkaller_version.name);
    if (err < 0)
        return err;

    // Slim the template if needed, otherwise copy over the one given
    string full_template = syzkaller_version.date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";;
    string new_template = bug.get_wd() + "/my_template.txt";
    vector<string> template_files = list_template_files(full_template);

    if (use_template.empty())
    {
        cout << "Slimming the template.\n";
        err = slim_template(bug.get_repro(), new_template, template_files);
        if (err < 0)
        {
            cout << "Error: failed to slim the template.\n";
            return err;
        }
        remove_template_files(template_files);
        copy(new_template, full_template);
        cout << SPACER;
    }
    else
    {
        remove_template_files(template_files);
        copy(use_template, full_template);
    }

    // Remove the flags that check for unused functions
    sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m64 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
            bug.get_syzdir() + "/Makefile");

    // Patch a boot error related to kvm
    if (syzkaller_version.date < Date(2021,1,1) && syzkaller_version.date >= Date(2020,5,1))
    {
        cout << "Applying build patch to Syzkaller.\n";
        sed_i("s/\\-enable\\-kvm \\-cpu host,migratable=off/\\-enable\\-kvm \\-cpu host/", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }

    // Apply patch for netfilter_bridge/ebtables
    if (syzkaller_version.date < Date(2018,9,27) && syzkaller_version.date >= Date(2018,2,17))
    {
        cout << "Applying netfilter_bridge patch to Syzkaller.\n";
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

    if (syzkaller_version.date >= Date(2020,10,12) && syzkaller_version.date <= Date(2020,10,13))
    {
        cout << "Applying cgroup mount patch to Syzkaller.\n";
        sed_i("s/failmsg(\\\"mount cgroup failed\\\", \\\"(%s, %s): %d\\\\n\\\", dir, enabled + 1, errno);/debug(\\\"mount(%s, %s) failed: %d\\\\n\\\", dir, enabled + 1, errno);/",
                    bug.get_syzdir() + "/executor/common_linux.h");
    }

    // Fix a build error with strncpy
    if (syzkaller_version.date < Date(2018,5,13) && syzkaller_version.date >= Date(2018,2,10))
    {
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/pkg/csource/linux_common.go");
    }

    // Handle old go mod
    if (check_file(bug.get_syzdir() + "/Godeps/Godeps.json") && !dangerzone)
    {
        cd(bug.get_syzdir());
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
        cd(inspector.get_inspect_dir());
    }

    // Fix issue with earlier versions of syzkaller
    if (check_file(bug.get_syzdir() + "/vendor/cloud.google.com/go/storage/not_go110.go"))
    {
        remove_file(bug.get_syzdir() + "/vendor/cloud.google.com/go/storage/not_go110.go");
    }

    // manually generate the template if needed
    if (!grep_to_find("descriptions:", bug.get_syzdir() + "/Makefile"))
    {
        cd(bug.get_syzdir());
        make(inspector.get_makeprocs(), "bin/syz-sysgen");
        char command[] = "./bin/syz-sysgen";
        char * arg_list[] = {command, nullptr};
        err = exec_and_wait("./bin/syz-sysgen", arg_list);
        if (err != 0)
            return -1;
        cd(inspector.get_inspect_dir());
    }

    // Build syzkaller
    cout << "Making Syzkaller.\n";
    cd(bug.get_syzdir());
    if (bug.get_arch() == "amd64")
        err = make(inspector.get_makeprocs());
    else
        err = syz_env_cross_compile(inspector.get_inspect_dir() + "/tools/syz-env");

    if (err < 0)
        cerr << "Error: Syzkaller failed to make.\n";
    cd(inspector.get_inspect_dir());

    return err;
}

int write_syzkaller_config(const Bug_Info &bug, const InspectorConfig &inspector, const VMConfig &vmc, Port_Info &p, const Date &syz_date)
{
    p.inc();

    ofstream outf;
    outf.open(bug.get_syzconfig());
    if (!outf)
    {
        cerr << "Error: Failed to open file " << bug.get_syzconfig() << ".\n";
        return -1;
    }

    outf << "{\n";

    // target was added on 2017-09-15
    if(syz_date > Date(2017,9,15)) 
    {
        outf << "    \"target\": \"linux/amd64" << (bug.get_arch() == "i386" ? "/386" : "") << "\",\n";
    }

    outf << "    \"http\": \"127.0.0.1:" << p.port << "\",\n"
         << "    \"workdir\": \"" << bug.get_kallerwd() << "\",\n";

    // "vmlinux" until 2018-06-27, then "kernel_obj" starting on 2018-06-28
    if (syz_date  >= Date(2018,6,28))
        outf << "    \"kernel_obj\": \"" << bug.get_kerneldir() << "\",\n";
    else
        outf << "    \"vmlinux\": \"" << bug.get_kerneldir() << "/vmlinux\",\n";

    // change image when syzkaller did. It shouldn't matter, but who knows.
    if (syz_date >= Date(2018,9,4))
        outf << "    \"image\": \"" << inspector.get_inspect_dir() << "/image/stretch/stretch.img\",\n"
             << "    \"sshkey\": \"" << inspector.get_inspect_dir() << "/image/stretch/stretch.id_rsa\",\n";
    else
        outf << "    \"image\": \"" << inspector.get_inspect_dir() << "/image/wheezy/wheezy.img\",\n"
             << "    \"sshkey\": \"" << inspector.get_inspect_dir() << "/image/wheezy/ssh/id_rsa\",\n";

    outf << "    \"syzkaller\": \"" << bug.get_syzdir() << "\",\n"
         << "    \"procs\": " << vmc.numProcs << ",\n"
         << "    \"type\": \"qemu\",\n"
         << "    \"reproduce\": false,\n"
         << "    \"vm\": {\n"
         << "        \"count\": " << vmc.numVM << ",\n"
         << "        \"kernel\": \"" << bug.get_kerneldir() << "/arch/x86/boot/bzImage\",\n"
         << "        \"cpu\": " << vmc.numCPU << ",\n"
         << "        \"mem\": " << inspector.get_mem() << "\n"
         << "    }\n"
         << "}\n";

    outf.close();
    return 0;
}

int insert_POC_as_seed(const Bug_Info &bug)
{
    // syz-db wants a directory with the seeds
    string to_pack = bug.get_wd() + "/to_pack";
    make_dir(to_pack);
    // watch if syzkaller complains about having the raw poc (with commants)
    copy(bug.get_repro(), to_pack);

    string corpus = bug.get_kallerwd() + "/corpus.db";
    // make sure to clear out the old corpus
    if (check_file(corpus))
        remove_file(corpus);

    // assumes syzkaller has already been made
    string com = bug.get_syzdir() + "/bin/syz-db";
    char * command = new char[com.size() + 1];
    strcpy(command, com.c_str());
    char arg1[] = "pack";
    char * arg2 = new char[to_pack.size() + 1];
    strcpy(arg2, to_pack.c_str());
    
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

int clean_syzkaller(const Bug_Info &bug)
{
    int pos0, err = 0;
    for (string file : list_dir(bug.get_syzdir()))
    {
        pos0 = file.find_last_of("/");
        if (file.at(pos0 + 1) != '.')
        {
            err = remove_dir(file);
            if (err != 0)
                return err;
        }
    }

    return 0;
}

int calc_bloat()
{
    return 0;
}
