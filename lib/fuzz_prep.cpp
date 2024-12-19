#include <bug_info.h>
#include <consts.h>
#include <date.h>
#include <environment.h>
#include <exec_api.h>
#include <file_api.h>
#include <fuzz.h>
#include <fuzz_prep.h>
#include <git.h>
#include <shell_api.h>
#include <template_parse.h>
#include <version.h>

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

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

VMConfig determine_threadedness(Environment &env, const Bug_Info &bug, std::ostream &logfile)
{
    string reproducer = bug.reproducer + "/repro-" + bug.numName + "-1.prog";
    int procs = get_procs_from_repro(reproducer);
    switch (procs)
    {
    case 1:
        cout << "Using resource allocation for a single proc bug.\n";
        logfile << "Single Proc Allocation\n";
        env.vmc = env.vmst;
        break;
    case 6:
        cout << "Using default resource allocation.\n";
        logfile << "Default Allocation.\n";
        env.vmc = env.vmd;
        break;
    case 8:
        cout << "Using resource allocation for a multi-proc bug.\n";
        logfile << "Multi-Proc Allocation.\n";
        env.vmc = env.vmr;
        break;
    default:
        cerr << "Warning: Could not retrieve number of procs from reproducer " << reproducer << ". Using Default.\n";
        env.vmc = env.vmd;
    }
    logfile << "VMs:" << env.vmc.numVM << endl
            << "CPUs:" << env.vmc.numCPU << endl
            << "Procs:" << env.vmc.numProcs << endl;

    return env.vmc;
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

string compiler_mux(const vector<Version> &versions, const Date &kernel_date, const Environment &env)
{
    int i;
    // Assumes list of versions is sorted least to greatest
    for (i = versions.size() - 1; i >= 0 && kernel_date < versions.at(i).date; i--);
    i = i < 0 ? 0 : i;

    return env.gcc_dir + versions.at(i).name;
}

string get_compiler(const vector<Version> &gcc_versions, const vector<Version> &clang_versions, const Date &kernel_date, const Environment &env)
{
    switch (env.compiler_setting)
    {
        case COMPILER_GCC:
            return compiler_mux(gcc_versions, kernel_date, env)+"/bin/gcc";
        case COMPILER_CLANG:
            return compiler_mux(clang_versions, kernel_date, env)+"/bin/clang";
        case COMPILER_CLANG_14:
            return "clang-14";
    }
    
    return "";
}

int clean_path(const string &old_path)
{
    return export_env("PATH=" + old_path);
}

// sets the specified config "con" in the config file "lines"
int set_config(const string &con, vector<string> &lines)
{
    string yes = con + "=y";
    for (int i = 0; i < lines.size(); i++)
    {
        if (lines.at(i).find(yes) != string::npos)
            return 0;
        else if (lines.at(i).find("# " + con) != string::npos)
        {
            lines.at(i) = yes;
            return 0;
        }
    }
    // This only works because make olddefconfig saves me (moves configs to correct spots)
    lines.push_back(yes);
    return 0;
}

// sets the specified config "con" in the config file "lines"
int unset_config(const string &con, vector<string> &lines)
{
    string yes = con + "=y";
    string unset = "# " + con + " is not set";
    for (int i = 0; i < lines.size(); i++)
    {
        if (lines.at(i).find(unset) != string::npos)
            return 0;
        else if (lines.at(i).find(yes) != string::npos)
        {
            lines.at(i) = unset;
            return 0;
        }
    }
    return 0;
}

int set_kernel_config(const string &config, const vector<string> &config_to_set)
{
    int err = 0;
    vector<string> lines;

    err = load_file(config, lines);
    if (err < 0)
        return err;

    for (string con : config_to_set)
        set_config(con, lines);

    err = write_file(config, lines);
    return err;
}

int unset_kernel_config(const string &config, const vector<string> &config_to_set)
{
    int err = 0;
    vector<string> lines;

    err = load_file(config, lines);
    if (err < 0)
        return err;

    for (string con : config_to_set)
        unset_config(con, lines);

    err = write_file(config, lines);
    return err;
}

void patch_kernel(const Environment &env, const Version &linux_version)
{
    string old_dir = pwd();
    cd(env.home);

    // apply patch from 760f8522ce08
    // Fixes "error: #error New address family defined, please update secclass_map."
    if (grep_to_find("#include <sys/socket.h>", env.kerneldir + "/scripts/selinux/mdp/mdp.c") &&
        grep_to_find("#include <sys/socket.h>", env.kerneldir + "/scripts/selinux/genheaders/genheaders.c") &&
        !grep_to_find("#include <sys/socket.h>", env.kerneldir + "/security/selinux/include/classmap.h"))
    {
        cout << "PATCH: Fixing includes in selinux/mpd and selinux/genheaders.\n";
        sed_i("s/#include <sys\\/socket.h>//", env.kerneldir + "/scripts/selinux/mdp/mdp.c");
        sed_i("s/#include <sys\\/socket.h>//", env.kerneldir + "/scripts/selinux/genheaders/genheaders.c");
        sed_i("s/#include <linux\\/capability.h>/#include <linux\\/capability.h>\\n#include <linux\\/socket.h>/", env.kerneldir + "/security/selinux/include/classmap.h");
    }

    // Add a patch for all of 14 commits
    if (linux_version.date == Date(2019,12,1))
    {
        cout << "PATCH: Fixing page size references in mm/userfaultfd.c.\n";
        sed_i("s/VM_BUG_ON(dst_addr \\& ~huge_page_mask(h));/VM_BUG_ON(dst_addr \\& (vma_hpagesize - 1));/", env.kerneldir + "/mm/userfaultfd.c");
        sed_i("s/dst_pte = huge_pte_alloc(dst_mm, dst_addr, huge_page_size(h));/dst_pte = huge_pte_alloc(dst_mm, dst_addr, vma_hpagesize);/", env.kerneldir + "/mm/userfaultfd.c");
        sed_i("s/pages_per_huge_page(h), true);/vma_hpagesize \\/ PAGE_SIZE, true);/", env.kerneldir + "/mm/userfaultfd.c");
    }

    // the date here gives rough estimate. Fix works before that date.
    if (!grep_to_find("ifdef CONFIG_X86_64", env.kerneldir + "/arch/x86/Makefile") &&
        linux_version.date <= Date(2018,6,9))
    {
        cout << "PATCH: Forcing 2MB page size in arch/x86/Makefile.\n";
        sed_i("s/LDFLAGS := \\-m elf_$(UTS_MACHINE)/LDFLAGS := \\-m elf_$(UTS_MACHINE)\\nifdef CONFIG_X86_64\\nLDFLAGS += $(call ld\\-option, \\-z max\\-page\\-size=0x200000)\\nendif\\n/",
                env.kerneldir + "/arch/x86/Makefile");
        
    }

    // Implement a patch from ea7b4244b3656ca33b19a950f092b5bbc718b40c
    // ~ 2021-07-31 to 2021-09-01 (merged to mainline on 2021-08-31)
    // Fixes "arch/x86/kernel/setup.c:916:6: error: implicit declaration of function ‘acpi_mps_check’"
    if (linux_version.date >= Date(2021,8,31) && linux_version.date <= Date(2021,9,1)
        && !grep_to_find("#include <linux\\/acpi\\.h>", env.kerneldir + "/arch/x86/kernel/setup.c"))
    {
        cout << "PATCH: Explicitly include acpi.h\n";
        sed_i("s/#include <linux\\/console.h>/#include <linux\\/acpi.h>\\n#include <linux\\/console.h>/", env.kerneldir + "/arch/x86/kernel/setup.c");
    }

    // Apply patch from bd74708cd979f4934f0744055ce3b47da68733ce
    // Revert "blackhole_netdev: fix syzkaller reported issue"
    if ((linux_version.date == Date(2019,10,15) || linux_version.date == Date(2019,10,16))
        && grep_to_find("struct inet6_dev \\*idev, \\*bdev;", env.kerneldir + "/net/ipv6/addrconf.c"))
    {
        cout << "PATCH: Fix regression in blackhole_netdev\n";
        sed_i("s/struct inet6_dev \\*idev, \\*bdev;/struct inet6_dev \\*idev;/", env.kerneldir + "/net/ipv6/addrconf.c");
        sed_i("/bdev = ipv6_add_dev(blackhole_netdev);/ d", env.kerneldir + "/net/ipv6/addrconf.c");
        sed_i("/} else if (IS_ERR(bdev)) {/,+2 d", env.kerneldir + "/net/ipv6/addrconf.c");
        sed_i("/addrconf_ifdown(blackhole_netdev, 2);/ d", env.kerneldir + "/net/ipv6/addrconf.c");

        sed_i("s/if (dev == net->loopback_dev)/struct net_device \\*loopback_dev = net->loopback_dev;\\nif (dev == loopback_dev)/", env.kerneldir + "/net/ipv6/route.c");
        sed_i("s/rt->rt6i_idev = in6_dev_get(blackhole_netdev);/rt->rt6i_idev = in6_dev_get(loopback_dev);/", env.kerneldir + "/net/ipv6/route.c");
        sed_i("/if (idev \\&\\& idev->dev != dev_net(dev)->loopback_dev) {/, +3 d", env.kerneldir + "/net/ipv6/route.c");
        sed_i("/struct inet6_dev \\*idev = rt->rt6i_idev;/r patches/linux-1.txt", env.kerneldir + "/net/ipv6/route.c");
    }

    // Apply Patch to fix boot error "VFS: Unable to mount root fs on unknown-block(8,0)"
    // patch from 79dede78c0573618e3137d3d8cbf78c84e25fabd
    if (linux_version.date <= Date(2020,5,8) && linux_version.date >= Date(2020,3,29)
        && grep_to_find("LSM_HOOK(int, 0, fs_context_parse_param, struct fs_context \\*fc,", env.kerneldir + "/include/linux/lsm_hook_defs.h"))
    {
        cout << "PATCH: Fix boot error \"VFS: Unable to mount root fs on unknown-block(8,0)\"\n";
        sed_i("s/LSM_HOOK(int, 0, fs_context_parse_param, struct fs_context \\*fc,/LSM_HOOK(int, -ENOPARAM, fs_context_parse_param, struct fs_context \\*fc,/",
                env.kerneldir + "/include/linux/lsm_hook_defs.h");
    }

    // Apply patch from 9f457179244a1c0316546b1760f8993d0d718861
    // fixes "WARNING: CPU: 0 PID: 0 at mm/memcontrol.c:5226 mem_cgroup_css_alloc+0x27a/0x860"
    // Also "boot error: WARNING in mem_cgroup_css_alloc"
    if (linux_version.date <= Date(2020,8,13) && linux_version.date >= Date(2020,8,12)
        && grep_to_find("\\/\\* We charge the parent cgroup, never the current task \\*\\/", env.kerneldir + "/mm/memcontrol.c"))
    {
        cout << "PATCH: Remove warning when allocating the root cgroup\n";
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", env.kerneldir + "/mm/memcontrol.c");
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", env.kerneldir + "/mm/memcontrol.c");
    }

    // KASAN: slab-out-of-bounds in hpet_alloc is known to trigger in the range 2020-01-23 to 2020-02-03
    // Patch: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=98c49f1746ac44ccc164e914b9a44183fad09f51
    // Guilty: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=987f028b8637cfa7658aa456ae73f8f21a7a7f6f
    // If it becomes a big issue, we can patch it.

    cd(old_dir);
}

int prep_kernel(const Environment &env, const Bug_Info &bug, Git &linux_git, const Version &linux_version, const std::string &compiler)
{
    int err = 0;
    cd(env.home);
    clean_kernel(env);
    linux_git.cleanup();

    // downloads the kernel version (does not decide)
    err = linux_git.fetch_and_checkout(linux_version.name);
    if (err < 0)
    {
        std::cerr << "Error: Failed to fetch/checkout linux finding commit\n" << std::flush;
        return -1;
    }

    // copy over the config
    copy(bug.kconfig, env.kerneldir + "/.config");

    // Handle Patches
    patch_kernel(env, linux_version);

    // build the kernel
    cout << "Building the kernel...\n" << flush;
    string outfile = env.logdir + bug.numName + "-kbuild.log";
    cd(env.kerneldir);
    err = make(env.makeprocs, {"olddefconfig", "CC="+compiler}, outfile);
    if (err < 0)
        return err;
    err = make(env.makeprocs, "CC="+compiler, outfile);
    if (err < 0)
    {
        cerr << "Error: The kernel failed to make.\n";
        return err;
    }
    cd(env.home);
    return err;
}

int clean_kernel(const Environment &env)
{
    string old_dir = pwd();
    int err = 0;

    cd(env.kerneldir);
    err = make(1, "clean");
    cd(old_dir);

    return (err == 0 ? 0 : -1);
}

void patch_syzkaller(const Environment &env, const Bug_Info &bug, const Version &syzkaller_version)
{
    string old_dir = pwd();
    cd(env.home);

    // Remove the flags that check for unused functions
    // Also remember to build execcutor with 32 bit flags for i386
    if (bug.arch == "i386")
    {
        sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m32 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
                env.syzdir + "/Makefile");
    }
    else
    {
        sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m64 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
                env.syzdir + "/Makefile");
    }

    // This sed is for older versions before e935237c9c7214eb37cb35a93c9930b590016094 (2019-01-19)
    // thankfully no overlap between the two checks, so we can just run both.
    sed_i("s/-pthread -Wall -Wframe-larger-than=8192 -Wparentheses -Werror/-pthread -Wall -Wframe-larger-than=8192 -Wparentheses/",
            env.syzdir + "/Makefile");

    // Patch a boot error related to kvm
    if (syzkaller_version.date < Date(2021,1,1) && syzkaller_version.date >= Date(2020,5,1))
    {
        cout << "PATCH: Removing migratable=off from qemu boot args.\n";
        sed_i("s/\\-enable\\-kvm \\-cpu host,migratable=off/\\-enable\\-kvm \\-cpu host/", env.syzdir + "/vm/qemu/qemu.go");
    }
    else if (syzkaller_version.date <= Date(2017,9,15) && grep_to_find("\\\"\\-enable\\-kvm\\\",", env.syzdir + "/vm/qemu/qemu.go"))
    {
        cout << "PATCH: Adding -cpu host to really old qemu boot args.\n";
        sed_i("s/\\\"\\-enable\\-kvm\\\",/\\\"\\-enable\\-kvm\\\", \\\"\\-cpu\\\", \\\"host,migratable=off\\\",/", env.syzdir + "/vm/qemu/qemu.go");
    }
    else if (syzkaller_version.date <= Date(2018,10,28))
    {
        cout << "PATCH: Adding -cpu host to qemu boot args.\n";
        sed_i("s/\\-enable\\-kvm/\\-enable\\-kvm \\-cpu host,migratable=off/", env.syzdir + "/vm/qemu/qemu.go");
    }

    if (syzkaller_version.date <= Date(2018,4,20) && syzkaller_version.date >= Date(2017,12,17))
    {
        cout << "PATCH: Fixing -smp in qemu boot args.\n";
        if (grep_to_find("Cpu", env.syzdir + "/vm/qemu/qemu.go"))
        {
            sed_i("/if inst.cfg.Cpu == 1/,+14d", env.syzdir + "/vm/qemu/qemu.go");
            sed_i("s/strconv.Itoa(inst.cfg.Mem),/strconv.Itoa(inst.cfg.Mem),\\n\\t\\\"\\-smp\\\", strconv.Itoa(inst.cfg.Cpu),/", env.syzdir + "/vm/qemu/qemu.go");
        }
        else
        {
            sed_i("/if inst.cfg.CPU == 1/,+14d", env.syzdir + "/vm/qemu/qemu.go");
            sed_i("s/strconv.Itoa(inst.cfg.Mem),/strconv.Itoa(inst.cfg.Mem),\\n\\t\\\"\\-smp\\\", strconv.Itoa(inst.cfg.CPU),/", env.syzdir + "/vm/qemu/qemu.go");
        }
    }

    if ( syzkaller_version.date <= Date(2018,4,16) &&
        grep_to_find(" \\-usb \\-usbdevice mouse \\-usbdevice tablet \\-soundhw all", env.syzdir + "/vm/qemu/qemu.go"))
    {
        cout << "PATCH: Removing usb/sound qemu boot args.\n";
        sed_i("s/ \\-usb \\-usbdevice mouse \\-usbdevice tablet \\-soundhw all//", env.syzdir + "/vm/qemu/qemu.go");
    }

    // Apply patch for netfilter_bridge/ebtables
    if (syzkaller_version.date < Date(2018,9,27) && syzkaller_version.date >= Date(2018,2,17))
    {
        cout << "PATCH: Fixing includes in netfilter_bridge.\n";
        sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-1.txt", env.syzdir + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", env.syzdir + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/if.h>//", env.syzdir + "/executor/common_linux.h");
        sed_i("s/#include <errno.h>/#include <errno.h>\\n#include <linux\\/if.h>/", env.syzdir + "/executor/common_linux.h");
        if (check_file(env.syzdir + "/pkg/csource/generated.go"))
        {
            sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-2.txt", env.syzdir + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", env.syzdir + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/if.h>//", env.syzdir + "/pkg/csource/generated.go");
            sed_i("s/#include <errno.h>/#include <errno.h>\\n#include <linux\\/if.h>/", env.syzdir + "/pkg/csource/generated.go");
        }
    }

    // runtime patch for file extraction (slab oob)
    if (syzkaller_version.date == Date(2018,9,26))
    {
        cout << "PATCH: Fixing slab OOB in pkg/report/linux.go.\n";
        sed_i("s/report := rep.Report\\[rep.StartPos:\\]/report := rep.Report\\[rep.reportPrefixLen:\\]/", env.syzdir + "/pkg/report/linux.go");
        sed_i("s/rep.Report = append(rep.Report, report...)/rep.reportPrefixLen = len(rep.Report)\\n\\trep.Report = append(rep.Report, report...)/", env.syzdir + "/pkg/report/linux.go");
        sed_i("s/guiltyFile string/guiltyFile string\\n\\treportPrefixLen int/", env.syzdir + "/pkg/report/report.go");
    }

    // patch for mounting cgroup
    if (syzkaller_version.date >= Date(2021,10,12) && syzkaller_version.date <= Date(2021,10,13))
    {
        cout << "PATCH: Fixing crash on cgroup mount.\n";
        sed_i("s/failmsg(\\\"mount cgroup failed\\\", \\\"(%s, %s): %d\\\\n\\\", dir, enabled + 1, errno);/debug(\\\"mount(%s, %s) failed: %d\\\\n\\\", dir, enabled + 1, errno);/",
                env.syzdir + "/executor/common_linux.h");
        sed_i("s/failmsg(\\\"mount cgroup failed\\\", \\\"(%s, %s): %d\\\\n\\\", dir, enabled + 1, errno);/debug(\\\"mount(%s, %s) failed: %d\\\\n\\\", dir, enabled + 1, errno);/",
                env.syzdir + "/pkg/csource/generated.go");
    }

    // Fix a build error with strncpy
    if (syzkaller_version.date < Date(2018,5,13) && syzkaller_version.date >= Date(2018,2,10))
    {
        cout << "PATCH: Fixing off by one in executor/common_linux.h.\n";
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", env.syzdir + "/executor/common_linux.h");
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", env.syzdir + "/pkg/csource/linux_common.go");
    }

    cd(old_dir);
}

int slim_syzkaller_template(const Environment &env, const Bug_Info &bug, const Version &syzkaller_version)
{
    int err = 0;
    string full_template = syzkaller_version.date < Date(2017,9,15) ? env.syzdir + "/sys" : env.syzdir + "/sys/linux";
    string new_template = env.wd + "my_template.txt";
    vector<string> template_files = list_template_files(full_template);
    cout << "Slimming the template.\n";
    err = slim_template(bug.allreproducer, new_template, template_files, syzkaller_version.date < OLD_INOUT_DATE);
    if (err < 0)
    {
        cout << "Error: failed to slim the template.\n";
        return err;
    }
    remove_template_files(template_files);
    copy(new_template, full_template);
    return err;
}

int prep_syzkaller(const Environment &env, const Bug_Info &bug, Git &syzkaller, const Version &syzkaller_version, bool do_slim)
{
    int err = 0;
    string outfile = env.logdir + bug.numName + "-syzbuild.log";

    syzkaller.cleanup();
    if (bug.arch == "i386")
    {
        if(syzkaller_version.date <= Date(2020,5,18))
        {
            cout << "Copying syz-env from " << env.home + "/tools/syz-env" << " to " << env.syzdir + "/tools/" << endl;
            move(env.syzdir + "/tools/syz-env/env.go", env.syzdir + "/tools/syz-env/make.go");
            move(env.syzdir + "/tools/syz-env", env.syzdir + "/tools/syz-make");
            sed_i("s/go run tools\\/syz-env\\/env\\.go))/go run tools\\/syz-make\\/make\\.go))/", env.syzdir + "/Makefile");
            copy(env.home + "/tools/syz-env", env.syzdir + "/tools/");
        }

        cd(env.syzdir);
        err = syz_env_clean(env.syzdir + "/tools/syz-env", bug);
        cd(env.home);
    }
    else
        err = clean_syzkaller(env);
    
    if (err < 0 || syzkaller.error() < 0)
        return err;

    // export targetvmarch and target arch if building for 386
    if (bug.arch == "i386")
    {
        export_env("TARGETVMARCH=amd64");
        export_env("TARGETARCH=386");
    }

    // work around time period where go mod tidy doesn't work
    bool dangerzone = false;
    if (syzkaller_version.date < Date(2020,7,4) && syzkaller_version.date >= Date(2020,4,30))
    {
        err = syzkaller.fetch_and_checkout("136082ab38d86932bc3ed0087694e99d0e55491b");
        if (err < 0)
            return err;

        cd(env.syzdir);
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
        cd(env.home);
        dangerzone = true;
    }

    // download syzkaller (does not decide)
    err = syzkaller.fetch_and_checkout(syzkaller_version.name);
    if (err < 0)
        return err;

    if (do_slim)
        slim_syzkaller_template(env, bug, syzkaller_version);

    patch_syzkaller(env, bug, syzkaller_version);

    // Handle old go mod
    if (check_file(env.syzdir + "/Godeps/Godeps.json") && !dangerzone)
    {
        cd(env.syzdir);
        go_mod_init();
        go_mod_tidy();
        go_mod_vendor();
        cd(env.home);
    }

    // Fix issue with earlier versions of syzkaller
    if (check_file(env.syzdir + "/vendor/cloud.google.com/go/storage/not_go110.go"))
    {
        remove_file(env.syzdir + "/vendor/cloud.google.com/go/storage/not_go110.go");
    }

    // manually generate the template if needed
    if (!grep_to_find("descriptions:", env.syzdir + "/Makefile"))
    {
        cd(env.syzdir);
        make(env.makeprocs, "bin/syz-sysgen");
        char command[] = "./bin/syz-sysgen";
        char * arg_list[] = {command, nullptr};
        err = exec_and_wait("./bin/syz-sysgen", arg_list);
        if (err != 0)
            return -1;
        cd(env.home);
    }

    if (syzkaller_version.date <= Date(2017,7,28))
    {
        cd(env.syzdir);
        make(env.makeprocs, "all-tools");
        cd(env.home);
    }

    // Build syzkaller
    cd(env.syzdir);
    if (bug.arch == "amd64")
        err = make(env.makeprocs, "", outfile);
    else
    {
        if(syzkaller_version.date <= Date(2020,5,18))
        {
            cout << "Copying syz-env from " << env.home + "/tools/syz-env" << " to " << env.syzdir + "/tools/" << endl;
            move(env.syzdir + "/tools/syz-env/env.go", env.syzdir + "/tools/syz-env/make.go");
            move(env.syzdir + "/tools/syz-env", env.syzdir + "/tools/syz-make");
            sed_i("s/go run tools\\/syz-env\\/env\\.go))/go run tools\\/syz-make\\/make\\.go))/", env.syzdir + "/Makefile");
            copy(env.home + "/tools/syz-env", env.syzdir + "/tools/");
        }

        err = syz_env_cross_compile(env.syzdir + "/tools/syz-env", bug);
    }

    if (err < 0)
        cerr << "Error: Syzkaller failed to make.\n";
    cd(env.home);

    return err;
}

int write_syzkaller_config(const Environment &env, const Bug_Info &bug, const Date &syz_date)
{
    ofstream outf;
    outf.open(env.syzconfig);
    if (!outf)
    {
        cerr << "Error: Failed to open file " << env.syzconfig << ".\n";
        return -1;
    }

    outf << "{\n";

    // target was added on 2017-09-15
    if(syz_date > Date(2017,9,15)) 
    {
        outf << "    \"target\": \"linux/amd64" << (bug.arch == "i386" ? "/386" : "") << "\",\n";
    }

    outf << "    \"http\": \"127.0.0.1:" << env.port.port << "\",\n"
         << "    \"workdir\": \"" << env.syzwd << "\",\n";

    // "vmlinux" until 2018-06-27, then "kernel_obj" starting on 2018-06-28
    if (syz_date  >= Date(2018,6,28))
        outf << "    \"kernel_obj\": \"" << env.kerneldir << "\",\n";
    else
        outf << "    \"vmlinux\": \"" << env.kerneldir << "/vmlinux\",\n";

    // change image when syzkaller did. It shouldn't matter, but who knows.
    if (syz_date >= Date(2018,9,4))
        outf << "    \"image\": \"" << env.image_dir << "/stretch/stretch.img\",\n"
             << "    \"sshkey\": \"" << env.image_dir << "/stretch/stretch.id_rsa\",\n";
    else
        outf << "    \"image\": \"" << env.image_dir << "/wheezy/wheezy.img\",\n"
             << "    \"sshkey\": \"" << env.image_dir << "/wheezy/ssh/id_rsa\",\n";

    outf << "    \"syzkaller\": \"" << env.syzdir << "\",\n"
         << "    \"procs\": " << env.vmc.numProcs << ",\n"
         << "    \"type\": \"qemu\",\n"
         << "    \"reproduce\": false,\n"
         << "    \"vm\": {\n"
         << "        \"count\": " << env.vmc.numVM << ",\n"
         << "        \"kernel\": \"" << env.kerneldir << "/arch/x86/boot/bzImage\",\n"
         << "        \"cpu\": " << env.vmc.numCPU << ",\n"
         << "        \"mem\": " << env.memory << "\n"
         << "    }\n"
         << "}\n";

    outf.close();
    return 0;
}

void reset_kaller_wd(const Environment &env)
{
    if (check_file(env.syzwd))
    {
        cout << "Reseting Syzkaller's working directory.\n";
        remove_dir(env.syzwd);
    }

    make_dir(env.syzwd);
    make_dir(env.syzwd + "/crashes");
    return;
}

// Calls syz-db (up)pack src dest.
// assumes syzkaller has already been made.
int syz_db(const Environment &env, const string &opt, const string &src, const string &dest)
{
    string com = env.syzdir + "/bin/syz-db";
    char * command = new char[com.size() + 1];
    strcpy(command, com.c_str());
    char * arg1 = new char[opt.size() + 1];
    strcpy(arg1, opt.c_str());
    char * arg2 = new char[src.size() + 1];
    strcpy(arg2, src.c_str());
    char * arg3 = new char[dest.size() + 1];
    strcpy(arg3, dest.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int ret = exec_and_wait(com, arg_list);

    delete[] command;
    delete[] arg1;
    delete[] arg2;
    delete[] arg3;
    return ret;
}

int syz_db_pack_corpus(const Environment &env, const string &corpusdir, const string &corpus)
{
    return syz_db(env, "pack", corpusdir, corpus);
}

int syz_db_unpack_corpus(const Environment &env, const string &corpusdir, const string &corpus)
{
    return syz_db(env, "unpack", corpus, corpusdir);
}

// Does the following as needed:
// Unpack the previous corpus
// Reset the working directory
// Filter/Clear the previous corpus
// Adds the PoCs to the corpus
// Packs the corpus
int prepare_kaller_wd(const Environment &env, const Bug_Info &bug, bool keep_corpus)
{
    bool do_pack = false;
    int err = 0;
    string corpus = env.syzwd + "corpus.db";
    string corpusdir = env.wd + "corpus";

    if (check_file(corpusdir))
        remove_dir(corpusdir);
    make_dir(corpusdir);

    if (keep_corpus && check_file(corpus))
    {
        err = syz_db_unpack_corpus(env, corpusdir, corpus);
        do_pack = true;
        if (err < 0)
            goto exit;
    }

    reset_kaller_wd(env);

    // Copy the reproducers into the corpus directory
    for (string repro : list_dir(bug.reproducer))
        copy(repro, corpusdir);
    do_pack = true;

    if (do_pack)
        err = syz_db_pack_corpus(env, corpusdir, corpus);

exit:
    if (check_file(corpusdir))
        remove_dir(corpusdir);
    return err;
}

int clean_syzkaller(const Environment &env)
{
    string old_dir = pwd();
    int err = 0;

    cd(env.syzdir);
    err = make(1, "clean");
    cd(old_dir);

    return (err == 0 ? 0 : -1);

    /*
    int pos0, err = 0;
    for (string file : list_dir(env.syzdir))
    {
        pos0 = file.find_last_of("/");
        if (file.at(pos0 + 1) != '.')
        {
            err = remove_dir(file);
            if (err != 0)
                return err;
        }
    }
    */
    //return 0;
}
