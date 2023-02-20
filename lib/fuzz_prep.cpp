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
#include <fuzz.h>

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
    string reproducer = bug.get_repro() + "/repro-bug" + to_string(bug.get_number()) + "-1.prog";
    int procs = get_procs_from_repro(reproducer);
    VMConfig vmc;
    switch (procs)
    {
    case 1:
        cout << "Using resource allocation for a single proc bug.\n";
        logfile << "Single Proc Allocation\n";
        vmc = inspector.get_vmst();
        break;
    case 6:
        cout << "Using default resource allocation.\n";
        logfile << "Default Allocation.\n";
        vmc = inspector.get_vmd();
        break;
    case 8:
        cout << "Using resource allocation for a multi-proc bug.\n";
        logfile << "Multi-Proc Allocation.\n";
        vmc = inspector.get_vmr();
        break;
    default:
        cerr << "Warning: Could not retrieve number of procs from reproducer " << reproducer << ". Using Default.\n";
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

void patch_kernel(const Bug_Info &bug, const InspectorConfig &inspector, const Version &linux_version)
{
    string old_dir = pwd();
    cd(inspector.get_inspect_dir());

    // apply patch from 760f8522ce08
    // Fixes "error: #error New address family defined, please update secclass_map."
    if (grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/scripts/selinux/mdp/mdp.c") &&
        grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/scripts/selinux/genheaders/genheaders.c") &&
        !grep_to_find("#include <sys/socket.h>", bug.get_kerneldir() + "/security/selinux/include/classmap.h"))
    {
        cout << "PATCH: Fixing includes in selinux/mpd and selinux/genheaders.\n";
        sed_i("s/#include <sys\\/socket.h>//", bug.get_kerneldir() + "/scripts/selinux/mdp/mdp.c");
        sed_i("s/#include <sys\\/socket.h>//", bug.get_kerneldir() + "/scripts/selinux/genheaders/genheaders.c");
        sed_i("s/#include <linux\\/capability.h>/#include <linux\\/capability.h>\\n#include <linux\\/socket.h>/", bug.get_kerneldir() + "/security/selinux/include/classmap.h");
    }

    // Add a patch for all of 14 commits
    if (linux_version.date == Date(2019,12,1))
    {
        cout << "PATCH: Fixing page size references in mm/userfaultfd.c.\n";
        sed_i("s/VM_BUG_ON(dst_addr \\& ~huge_page_mask(h));/VM_BUG_ON(dst_addr \\& (vma_hpagesize - 1));/", bug.get_kerneldir() + "/mm/userfaultfd.c");
        sed_i("s/dst_pte = huge_pte_alloc(dst_mm, dst_addr, huge_page_size(h));/dst_pte = huge_pte_alloc(dst_mm, dst_addr, vma_hpagesize);/", bug.get_kerneldir() + "/mm/userfaultfd.c");
        sed_i("s/pages_per_huge_page(h), true);/vma_hpagesize \\/ PAGE_SIZE, true);/", bug.get_kerneldir() + "/mm/userfaultfd.c");
    }

    // the date here gives rough estimate. Fix works before that date.
    if (!grep_to_find("ifdef CONFIG_X86_64", bug.get_kerneldir() + "/arch/x86/Makefile") &&
        linux_version.date <= Date(2018,6,9))
    {
        cout << "PATCH: Forcing 2MB page size in arch/x86/Makefile.\n";
        sed_i("s/LDFLAGS := \\-m elf_$(UTS_MACHINE)/LDFLAGS := \\-m elf_$(UTS_MACHINE)\\nifdef CONFIG_X86_64\\nLDFLAGS += $(call ld\\-option, \\-z max\\-page\\-size=0x200000)\\nendif\\n/",
                bug.get_kerneldir() + "/arch/x86/Makefile");
        
    }

    // Implement a patch from ea7b4244b3656ca33b19a950f092b5bbc718b40c
    // ~ 2021-07-31 to 2021-09-01 (merged to mainline on 2021-08-31)
    // Fixes "arch/x86/kernel/setup.c:916:6: error: implicit declaration of function ‘acpi_mps_check’"
    if (linux_version.date >= Date(2021,8,31) && linux_version.date <= Date(2021,9,1)
        && !grep_to_find("#include <linux\\/acpi\\.h>", bug.get_kerneldir() + "/arch/x86/kernel/setup.c"))
    {
        cout << "PATCH: Explicitly include acpi.h\n";
        sed_i("s/#include <linux\\/console.h>/#include <linux\\/acpi.h>\\n#include <linux\\/console.h>/", bug.get_kerneldir() + "/arch/x86/kernel/setup.c");
    }

    // Apply patch from bd74708cd979f4934f0744055ce3b47da68733ce
    // Revert "blackhole_netdev: fix syzkaller reported issue"
    if ((linux_version.date == Date(2019,10,15) || linux_version.date == Date(2019,10,16))
        && grep_to_find("struct inet6_dev \\*idev, \\*bdev;", bug.get_kerneldir() + "/net/ipv6/addrconf.c"))
    {
        cout << "PATCH: Fix regression in blackhole_netdev\n";
        sed_i("s/struct inet6_dev \\*idev, \\*bdev;/struct inet6_dev \\*idev;/", bug.get_kerneldir() + "/net/ipv6/addrconf.c");
        sed_i("/bdev = ipv6_add_dev(blackhole_netdev);/ d", bug.get_kerneldir() + "/net/ipv6/addrconf.c");
        sed_i("/} else if (IS_ERR(bdev)) {/,+2 d", bug.get_kerneldir() + "/net/ipv6/addrconf.c");
        sed_i("/addrconf_ifdown(blackhole_netdev, 2);/ d", bug.get_kerneldir() + "/net/ipv6/addrconf.c");

        sed_i("s/if (dev == net->loopback_dev)/struct net_device \\*loopback_dev = net->loopback_dev;\\nif (dev == loopback_dev)/", bug.get_kerneldir() + "/net/ipv6/route.c");
        sed_i("s/rt->rt6i_idev = in6_dev_get(blackhole_netdev);/rt->rt6i_idev = in6_dev_get(loopback_dev);/", bug.get_kerneldir() + "/net/ipv6/route.c");
        sed_i("/if (idev \\&\\& idev->dev != dev_net(dev)->loopback_dev) {/, +3 d", bug.get_kerneldir() + "/net/ipv6/route.c");
        sed_i("/struct inet6_dev \\*idev = rt->rt6i_idev;/r patches/linux-1.txt", bug.get_kerneldir() + "/net/ipv6/route.c");
    }

    // Apply Patch to fix boot error "VFS: Unable to mount root fs on unknown-block(8,0)"
    // patch from 79dede78c0573618e3137d3d8cbf78c84e25fabd
    if (linux_version.date <= Date(2020,5,8) && linux_version.date >= Date(2020,3,29)
        && grep_to_find("LSM_HOOK(int, 0, fs_context_parse_param, struct fs_context \\*fc,", bug.get_kerneldir() + "/include/linux/lsm_hook_defs.h"))
    {
        cout << "PATCH: Fix boot error \"VFS: Unable to mount root fs on unknown-block(8,0)\"\n";
        sed_i("s/LSM_HOOK(int, 0, fs_context_parse_param, struct fs_context \\*fc,/LSM_HOOK(int, -ENOPARAM, fs_context_parse_param, struct fs_context \\*fc,/",
                bug.get_kerneldir() + "/include/linux/lsm_hook_defs.h");
    }

    // Apply patch from 9f457179244a1c0316546b1760f8993d0d718861
    // fixes "WARNING: CPU: 0 PID: 0 at mm/memcontrol.c:5226 mem_cgroup_css_alloc+0x27a/0x860"
    // Also "boot error: WARNING in mem_cgroup_css_alloc"
    if (linux_version.date <= Date(2020,8,13) && linux_version.date >= Date(2020,8,12)
        && grep_to_find("\\/\\* We charge the parent cgroup, never the current task \\*\\/", bug.get_kerneldir() + "/mm/memcontrol.c"))
    {
        cout << "PATCH: Remove warning when allocating the root cgroup\n";
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", bug.get_kerneldir() + "/mm/memcontrol.c");
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", bug.get_kerneldir() + "/mm/memcontrol.c");
    }

    cd(old_dir);
}

int prep_kernel(const Bug_Info &bug, const InspectorConfig &inspector, const Version &linux_version, const string &repo) // repoistory &repo, const hash
{
    int err = 0;
    cd(inspector.get_inspect_dir());

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
    patch_kernel(bug, inspector, linux_version);

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

void patch_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const Version &syzkaller_version)
{
    string old_dir = pwd();
    cd(inspector.get_inspect_dir());

    // Patch a boot error related to kvm
    if (syzkaller_version.date < Date(2021,1,1) && syzkaller_version.date >= Date(2020,5,1))
    {
        cout << "PATCH: Removing migratable=off from qemu boot args.\n";
        sed_i("s/\\-enable\\-kvm \\-cpu host,migratable=off/\\-enable\\-kvm \\-cpu host/", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }
    else if (syzkaller_version.date <= Date(2017,9,15) && grep_to_find("\\\"\\-enable\\-kvm\\\",", bug.get_syzdir() + "/vm/qemu/qemu.go"))
    {
        cout << "PATCH: Adding -cpu host to really old qemu boot args.\n";
        sed_i("s/\\\"\\-enable\\-kvm\\\",/\\\"\\-enable\\-kvm\\\", \\\"\\-cpu\\\", \\\"host,migratable=off\\\",/", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }
    else if (syzkaller_version.date <= Date(2018,10,28))
    {
        cout << "PATCH: Adding -cpu host to qemu boot args.\n";
        sed_i("s/\\-enable\\-kvm/\\-enable\\-kvm \\-cpu host,migratable=off/", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }

    if (syzkaller_version.date <= Date(2018,4,20) && syzkaller_version.date >= Date(2017,12,17))
    {
        cout << "PATCH: Fixing -smp in qemu boot args.\n";
        if (grep_to_find("Cpu", bug.get_syzdir() + "/vm/qemu/qemu.go"))
        {
            sed_i("/if inst.cfg.Cpu == 1/,+14d", bug.get_syzdir() + "/vm/qemu/qemu.go");
            sed_i("s/strconv.Itoa(inst.cfg.Mem),/strconv.Itoa(inst.cfg.Mem),\\n\\t\\\"\\-smp\\\", strconv.Itoa(inst.cfg.Cpu),/", bug.get_syzdir() + "/vm/qemu/qemu.go");
        }
        else
        {
            sed_i("/if inst.cfg.CPU == 1/,+14d", bug.get_syzdir() + "/vm/qemu/qemu.go");
            sed_i("s/strconv.Itoa(inst.cfg.Mem),/strconv.Itoa(inst.cfg.Mem),\\n\\t\\\"\\-smp\\\", strconv.Itoa(inst.cfg.CPU),/", bug.get_syzdir() + "/vm/qemu/qemu.go");
        }
    }

    if ( syzkaller_version.date <= Date(2018,4,16) &&
        grep_to_find(" \\-usb \\-usbdevice mouse \\-usbdevice tablet \\-soundhw all", bug.get_syzdir() + "/vm/qemu/qemu.go"))
    {
        cout << "PATCH: Removing usb/sound qemu boot args.\n";
        sed_i("s/ \\-usb \\-usbdevice mouse \\-usbdevice tablet \\-soundhw all//", bug.get_syzdir() + "/vm/qemu/qemu.go");
    }

    // Apply patch for netfilter_bridge/ebtables
    if (syzkaller_version.date < Date(2018,9,27) && syzkaller_version.date >= Date(2018,2,17))
    {
        cout << "PATCH: Fixing includes in netfilter_bridge.\n";
        sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-1.txt", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <linux\\/if.h>//", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/#include <errno.h>/#include <errno.h>\\n#include <linux\\/if.h>/", bug.get_syzdir() + "/executor/common_linux.h");
        if (check_file(bug.get_syzdir() + "/pkg/csource/generated.go"))
        {
            sed_i("/#include <linux\\/netfilter_bridge\\/ebtables.h>/r patches/syz-2.txt", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/netfilter_bridge\\/ebtables.h>//", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <linux\\/if.h>//", bug.get_syzdir() + "/pkg/csource/generated.go");
            sed_i("s/#include <errno.h>/#include <errno.h>\\n#include <linux\\/if.h>/", bug.get_syzdir() + "/pkg/csource/generated.go");
        }
    }

    // runtime patch for file extraction (slab oob)
    if (syzkaller_version.date == Date(2018,9,26))
    {
        cout << "PATCH: Fixing slab OOB in pkg/report/linux.go.\n";
        sed_i("s/report := rep.Report\\[rep.StartPos:\\]/report := rep.Report\\[rep.reportPrefixLen:\\]/", bug.get_syzdir() + "/pkg/report/linux.go");
        sed_i("s/rep.Report = append(rep.Report, report...)/rep.reportPrefixLen = len(rep.Report)\\n\\trep.Report = append(rep.Report, report...)/", bug.get_syzdir() + "/pkg/report/linux.go");
        sed_i("s/guiltyFile string/guiltyFile string\\n\\treportPrefixLen int/", bug.get_syzdir() + "/pkg/report/report.go");
    }

    // patch for mounting cgroup
    if (syzkaller_version.date >= Date(2021,10,12) && syzkaller_version.date <= Date(2021,10,13))
    {
        cout << "PATCH: Fixing crash on cgroup mount.\n";
        sed_i("s/failmsg(\\\"mount cgroup failed\\\", \\\"(%s, %s): %d\\\\n\\\", dir, enabled + 1, errno);/debug(\\\"mount(%s, %s) failed: %d\\\\n\\\", dir, enabled + 1, errno);/",
                bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/failmsg(\\\"mount cgroup failed\\\", \\\"(%s, %s): %d\\\\n\\\", dir, enabled + 1, errno);/debug(\\\"mount(%s, %s) failed: %d\\\\n\\\", dir, enabled + 1, errno);/",
                bug.get_syzdir() + "/pkg/csource/generated.go");
    }

    // Fix a build error with strncpy
    if (syzkaller_version.date < Date(2018,5,13) && syzkaller_version.date >= Date(2018,2,10))
    {
        cout << "PATCH: Fixing off by one in executor/common_linux.h.\n";
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/executor/common_linux.h");
        sed_i("s/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf)));/NONFAILING(strncpy(buf, (char\\*)a0, sizeof(buf) - 1));/", bug.get_syzdir() + "/pkg/csource/linux_common.go");
    }

    cd(old_dir);
}

int prep_syzkaller(const Bug_Info &bug, const InspectorConfig &inspector, const Version &syzkaller_version, const string &use_template)
{
    int err = 0;
    if (bug.get_arch() == "i386")
    {
        if(syzkaller_version.date <= Date(2020,5,18))
        {
            cout << "Copying syz-env from " << inspector.get_inspect_dir() + "/tools/syz-env" << " to " << bug.get_syzdir() + "/tools/" << endl;
            move(bug.get_syzdir() + "/tools/syz-env/env.go", bug.get_syzdir() + "/tools/syz-env/make.go");
            move(bug.get_syzdir() + "/tools/syz-env", bug.get_syzdir() + "/tools/syz-make");
            sed_i("s/go run tools\\/syz-env\\/env\\.go))/go run tools\\/syz-make\\/make\\.go))/", bug.get_syzdir() + "/Makefile");
            copy(inspector.get_inspect_dir() + "/tools/syz-env", bug.get_syzdir() + "/tools/");
        }

        cd(bug.get_syzdir());
        err = syz_env_clean(bug.get_syzdir() + "/tools/syz-env", bug);
        cd(inspector.get_inspect_dir());
    }
    
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
    string full_template = syzkaller_version.date < Date(2017,9,15) ? bug.get_syzdir() + "/sys" : bug.get_syzdir() + "/sys/linux";
    string new_template = bug.get_wd() + "/my_template.txt";
    vector<string> template_files = list_template_files(full_template);

    if (use_template.empty())
    {
        cout << "Slimming the template.\n";
        err = slim_template(bug.get_allrepro(), new_template, template_files, syzkaller_version.date < OLD_INOUT_DATE);
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
    // Also remember to build execcutor with 32 bit flags for i386
    if (bug.get_arch() == "i386")
    {
        sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m32 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
                bug.get_syzdir() + "/Makefile");
    }
    else
    {
        sed_i("s/$(ADDCFLAGS) $(CFLAGS) -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/-m64 -O2 -pthread -Wall -static-pie -DGOOS_$(TARGETOS)=1 -DGOARCH_$(TARGETARCH)=1/",
                bug.get_syzdir() + "/Makefile");
    }

    // This sed is for older versions before e935237c9c7214eb37cb35a93c9930b590016094 (2019-01-19)
    // thankfully no overlap between the two checks, so we can just run both.
    sed_i("s/-pthread -Wall -Wframe-larger-than=8192 -Wparentheses -Werror/-pthread -Wall -Wframe-larger-than=8192 -Wparentheses/",
            bug.get_syzdir() + "/Makefile");

    patch_syzkaller(bug, inspector, syzkaller_version);

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

    if (syzkaller_version.date <= Date(2017,7,28))
    {
        cout << "Making all-tools.\n";
        cd(bug.get_syzdir());
        make(inspector.get_makeprocs(), "all-tools");
        cd(inspector.get_inspect_dir());
    }

    // Build syzkaller
    cout << "Making Syzkaller.\n";
    cd(bug.get_syzdir());
    if (bug.get_arch() == "amd64")
        err = make(inspector.get_makeprocs());
    else
    {
        if(syzkaller_version.date <= Date(2020,5,18))
        {
            cout << "Copying syz-env from " << inspector.get_inspect_dir() + "/tools/syz-env" << " to " << bug.get_syzdir() + "/tools/" << endl;
            move(bug.get_syzdir() + "/tools/syz-env/env.go", bug.get_syzdir() + "/tools/syz-env/make.go");
            move(bug.get_syzdir() + "/tools/syz-env", bug.get_syzdir() + "/tools/syz-make");
            sed_i("s/go run tools\\/syz-env\\/env\\.go))/go run tools\\/syz-make\\/make\\.go))/", bug.get_syzdir() + "/Makefile");
            copy(inspector.get_inspect_dir() + "/tools/syz-env", bug.get_syzdir() + "/tools/");
        }

        err = syz_env_cross_compile(bug.get_syzdir() + "/tools/syz-env", bug);
    }

    if (err < 0)
        cerr << "Error: Syzkaller failed to make.\n";
    cd(inspector.get_inspect_dir());

    return err;
}

int write_syzkaller_config(const Bug_Info &bug, const InspectorConfig &inspector, const VMConfig &vmc, Port_Info &p, const Date &syz_date)
{
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
    string corpus = bug.get_kallerwd() + "/corpus.db";
    // make sure to clear out the old corpus
    if (check_file(corpus))
        remove_file(corpus);

    // assumes syzkaller has already been made
    string com = bug.get_syzdir() + "/bin/syz-db";
    char * command = new char[com.size() + 1];
    strcpy(command, com.c_str());
    char arg1[] = "pack";
    char * arg2 = new char[bug.get_repro().size() + 1];
    strcpy(arg2, bug.get_repro().c_str());
    
    char * arg3 = new char[corpus.size() + 1];
    strcpy(arg3, corpus.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int ret = exec_and_wait(com, arg_list);

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
