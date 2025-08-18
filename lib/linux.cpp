#include <consts.h>
#include <date.h>
#include <environment.h>
#include <exec_api.h>
#include <file_api.h>
#include <fuzz.h>
#include <linux.h>
#include <git.h>
#include <make.h>
#include <my_string.h>
#include <shell_api.h>
#include <template_parse.h>
#include <version.h>

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#include <string.h>
#include <unistd.h>

Git prep_kernel_local_repo(Environment &env)
{
    Git linux_git(env.kerneldir, env.repository, env.branch);
    return linux_git;
}

// sets the specified config "con" in the config file "lines"
int set_config(const std::string &con, std::vector<std::string> &lines)
{
    std::string yes = con + "=y";
    for (int i = 0; i < lines.size(); i++)
    {
        if (lines.at(i).find(yes) != std::string::npos)
            return 0;
        else if (lines.at(i).find("# " + con) != std::string::npos)
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
int unset_config(const std::string &con, std::vector<std::string> &lines)
{
    std::string yes = con + "=y";
    std::string unset = "# " + con + " is not set";
    for (int i = 0; i < lines.size(); i++)
    {
        if (lines.at(i).find(unset) != std::string::npos)
            return 0;
        else if (lines.at(i).find(yes) != std::string::npos)
        {
            lines.at(i) = unset;
            return 0;
        }
    }
    return 0;
}

int set_kernel_config(const std::string &config, const std::vector<std::string> &config_to_set)
{
    int err = 0;
    std::vector<std::string> lines;

    err = load_file(config, lines);
    if (err < 0)
        return err;

    for (std::string con : config_to_set)
        set_config(con, lines);

    err = write_file(config, lines);
    return err;
}

int unset_kernel_config(const std::string &config, const std::vector<std::string> &config_to_set)
{
    int err = 0;
    std::vector<std::string> lines;

    err = load_file(config, lines);
    if (err < 0)
        return err;

    for (std::string con : config_to_set)
        unset_config(con, lines);

    err = write_file(config, lines);
    return err;
}

// Applies known custom patches to the kernel
void patch_kernel(const Environment &env, const Version &linux_version)
{
    // apply patch from 760f8522ce08
    // Fixes "error: #error New address family defined, please update secclass_map."
    if (grep_to_find("#include <sys/socket.h>", env.kerneldir + "/scripts/selinux/mdp/mdp.c") &&
        grep_to_find("#include <sys/socket.h>", env.kerneldir + "/scripts/selinux/genheaders/genheaders.c") &&
        !grep_to_find("#include <sys/socket.h>", env.kerneldir + "/security/selinux/include/classmap.h"))
    {
        std::cout << "PATCH: Fixing includes in selinux/mpd and selinux/genheaders.\n";
        sed_i("s/#include <sys\\/socket.h>//", env.kerneldir + "/scripts/selinux/mdp/mdp.c");
        sed_i("s/#include <sys\\/socket.h>//", env.kerneldir + "/scripts/selinux/genheaders/genheaders.c");
        sed_i("s/#include <linux\\/capability.h>/#include <linux\\/capability.h>\\n#include <linux\\/socket.h>/", env.kerneldir + "/security/selinux/include/classmap.h");
    }

    // Add a patch for all of 14 commits
    if (linux_version.date == Date(2019,12,1))
    {
        std::cout << "PATCH: Fixing page size references in mm/userfaultfd.c.\n";
        sed_i("s/VM_BUG_ON(dst_addr \\& ~huge_page_mask(h));/VM_BUG_ON(dst_addr \\& (vma_hpagesize - 1));/", env.kerneldir + "/mm/userfaultfd.c");
        sed_i("s/dst_pte = huge_pte_alloc(dst_mm, dst_addr, huge_page_size(h));/dst_pte = huge_pte_alloc(dst_mm, dst_addr, vma_hpagesize);/", env.kerneldir + "/mm/userfaultfd.c");
        sed_i("s/pages_per_huge_page(h), true);/vma_hpagesize \\/ PAGE_SIZE, true);/", env.kerneldir + "/mm/userfaultfd.c");
    }

    // the date here gives rough estimate. Fix works before that date.
    if (!grep_to_find("ifdef CONFIG_X86_64", env.kerneldir + "/arch/x86/Makefile") &&
        linux_version.date <= Date(2018,6,9))
    {
        std::cout << "PATCH: Forcing 2MB page size in arch/x86/Makefile.\n";
        sed_i("s/LDFLAGS := \\-m elf_$(UTS_MACHINE)/LDFLAGS := \\-m elf_$(UTS_MACHINE)\\nifdef CONFIG_X86_64\\nLDFLAGS += $(call ld\\-option, \\-z max\\-page\\-size=0x200000)\\nendif\\n/",
            env.kerneldir + "/arch/x86/Makefile");
        
    }

    // Implement a patch from ea7b4244b3656ca33b19a950f092b5bbc718b40c
    // ~ 2021-07-31 to 2021-09-01 (merged to mainline on 2021-08-31)
    // Fixes "arch/x86/kernel/setup.c:916:6: error: implicit declaration of function ‘acpi_mps_check’"
    if (linux_version.date >= Date(2021,8,31) && linux_version.date <= Date(2021,9,1)
        && !grep_to_find("#include <linux\\/acpi\\.h>", env.kerneldir + "/arch/x86/kernel/setup.c"))
    {
        std::cout << "PATCH: Explicitly include acpi.h\n";
        sed_i("s/#include <linux\\/console.h>/#include <linux\\/acpi.h>\\n#include <linux\\/console.h>/", env.kerneldir + "/arch/x86/kernel/setup.c");
    }

    // Apply patch from bd74708cd979f4934f0744055ce3b47da68733ce
    // Revert "blackhole_netdev: fix syzkaller reported issue"
    if ((linux_version.date == Date(2019,10,15) || linux_version.date == Date(2019,10,16))
        && grep_to_find("struct inet6_dev \\*idev, \\*bdev;", env.kerneldir + "/net/ipv6/addrconf.c"))
    {
        std::cout << "PATCH: Fix regression in blackhole_netdev\n";
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
        std::cout << "PATCH: Fix boot error \"VFS: Unable to mount root fs on unknown-block(8,0)\"\n";
        sed_i("s/LSM_HOOK(int, 0, fs_context_parse_param, struct fs_context \\*fc,/LSM_HOOK(int, -ENOPARAM, fs_context_parse_param, struct fs_context \\*fc,/",
            env.kerneldir + "/include/linux/lsm_hook_defs.h");
    }

    // Apply patch from 9f457179244a1c0316546b1760f8993d0d718861
    // fixes "WARNING: CPU: 0 PID: 0 at mm/memcontrol.c:5226 mem_cgroup_css_alloc+0x27a/0x860"
    // Also "boot error: WARNING in mem_cgroup_css_alloc"
    if (linux_version.date <= Date(2020,8,13) && linux_version.date >= Date(2020,8,12)
        && grep_to_find("\\/\\* We charge the parent cgroup, never the current task \\*\\/", env.kerneldir + "/mm/memcontrol.c"))
    {
        std::cout << "PATCH: Remove warning when allocating the root cgroup\n";
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", env.kerneldir + "/mm/memcontrol.c");
        sed_i("/\\/\\* We charge the parent cgroup, never the current task \\*\\//,+1 d", env.kerneldir + "/mm/memcontrol.c");
    }

    // KASAN: slab-out-of-bounds in hpet_alloc is known to trigger in the range 2020-01-23 to 2020-02-03
    // Patch: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=98c49f1746ac44ccc164e914b9a44183fad09f51
    // Guilty: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/commit/?id=987f028b8637cfa7658aa456ae73f8f21a7a7f6f
    // If it becomes a big issue, we can patch it.
}

std::string canonical_title(const std::string &title)
{
    return title;
}

// Apply the same backports as syz-bisect
void apply_backports(const Environment &env, Git &linux_git, const Version &linux_version)
{
    // Here are patches and notes from the Syzkaller team.
    std::vector<Backport> backports = {
        // Compiling v4.6..v5.11 with a modern objtool, w/o this patch, results in the
        // following issue, when compiling with clang:
        // arch/x86/entry/thunk_64.o: warning: objtool: missing symbol table
        // We don't bisect that far back with neither clang nor gcc, so this should be fine:
        Backport("", "1d489151e9f9d1647110277ff77282fe4d96d09b", "objtool: Don't fail on missing symbol table"),
        
        // With newer compiler versions, kernel compilation fails with:
        // subcmd-util.h:56:23: error: pointer may be used after ‘realloc’ [-Werror=use-after-free]
        // 56 |                 ret = realloc(ptr, size);
        // The guilty commit is from 2015, we don't bisect that far.
        Backport("", "52a9dab6d892763b2a8334a568bd4e2c1a6fde66", "libsubcmd: Fix use-after-free for realloc(..., 0)"),

        // A number of old releases fail with KASAN: use-after-free in task_active_pid_ns.
        // The problem was actually present so long ago that we do not need to check whether
        // the guilty commit is present. We don't bisect that back (v2.*) anyway.
        Backport("", "0711f0d7050b9e07c44bc159bbc64ac0a1022c7f", "pid: take a reference when initializing `cad_pid`"),

        // Fixes the following error:
        // check.c:2865:58: error: '%d' directive output may be truncated writing between 1 and
        // 10 bytes into a region of size 9 [-Werror=format-truncation=]
        Backport("db2b0c5d7b6f19b3c2cab08c531b65342eb5252b", "82880283d7fcd0a1d20964a56d6d1a5cc0df0713", "objtool: Fix truncated string warning"),

        // Fixes `boot failed: WARNING in kvm_wait`.
        Backport("997acaf6b4b59c6a9c259740312a69ea549cc684", "f4e61f0c9add3b00bd5f2df3c814d688849b8707", "x86/kvm: Fix broken irq restoration in kvm_wait"),

        // Fixes `error: implicit declaration of function 'acpi_mps_check'`.
        Backport("342f43af70dbc74f8629381998f92c060e1763a2", "ea7b4244b3656ca33b19a950f092b5bbc718b40c", "x86/setup: Explicitly include acpi.h"),

        // Fixes `BUG: KASAN: slab-use-after-free in binder_add_device` at boot.
        Backport("12d909cac1e1c4147cc3417fee804ee12fc6b984", "e77aff5528a183462714f750e45add6cc71e276a", "binderfs: fix use-after-free in binder_devices"),

        // Fixes `unregister_netdevice: waiting for batadv0 to become free. Usage count = 3`.
        // Several v6.15-rc* tags are essentially unfuzzeable because of this.
        Backport("00b35530811f2aa3d7ceec2dbada80861c7632a8", "10a77965760c6e2b3eef483be33ae407004df894", "batman-adv: Fix double-hold of meshif when getting enabled")
    };

    for (Backport bp : backports)
    {
        // If the guilty commit is not an ancestor, no need to patch.
        if (!bp.guilty_hash.empty() && !linux_git.is_ancestor(bp.guilty_hash, linux_version.id))
            continue;

        // If the fix is present, also do not patch.
        if (linux_git.commit_exists_by_title(bp.title, linux_version.id))
            continue;

        std::cout << "PATCH: " << bp.title << "\n" << std::flush;
        if (linux_git.cherry_pick(bp.fix_hash) != 0)
            std::cerr << "Failed to apply patch: " << bp.title << ". Continuing...\n" << std::flush;
    }

    return;
}

int build_kernel(const Environment &env, Git &linux_git, const Version &linux_version, const std::string &compiler, bool bisecting)
{
    int err = 0;
    std::string old_dir = pwd();
    clean_kernel(env);
    linux_git.cleanup();

    // downloads the kernel version (does not decide)
    // if Bisecting, git bisect has already checked out the version
    if (!bisecting)
    {
        err = linux_git.fetch_and_checkout(linux_version.id);
        if (err < 0)
        {
            std::cerr << "Error: Failed to fetch/checkout linux finding commit\n" << std::flush;
            return -1;
        }
    }
    

    // copy over the config
    copy(env.kconfig, env.kerneldir + ".config");

    apply_backports(env, linux_git, linux_version);

    // Handle Patches
    if (env.feats.patch_kernel)
        patch_kernel(env, linux_version);

    // TODO: apply_backports

    // build the kernel
    cd(env.kerneldir);
    err = make(env.makeprocs, {"olddefconfig", "CC="+compiler}, env.kbuildlog());
    if (err < 0)
        return err;
    err = make(env.makeprocs, "CC="+compiler, env.kbuildlog());
    if (err < 0)
    {
        std::cerr << "Error: The kernel failed to make.\n";
        return err;
    }
    cd(old_dir);
    return err;
}

int clean_kernel(const Environment &env)
{
    std::string old_dir = pwd();
    int err = 0;

    cd(env.kerneldir);
    err = make(1, "clean", env.kbuildlog());
    cd(old_dir);

    return (err == 0 ? 0 : -1);
}

// Tries to remove the config related to the sanitizer finding the blocking bug
// TODO: add more sanitizers
int remove_related_config(const Environment &env, const std::string &bb)
{
    int err = 0;
    std::string sanitizer = split(bb, ' ').front();
    // If any duplicates need the sanitizer, don't remove it
    for (std::string dup : env.duplicates)
        if (sanitizer.find(split(dup, ' ').front()) != std::string::npos)
            return 0;

    if (sanitizer.find("UBSAN") != std::string::npos)
    {
        err = unset_kernel_config(env.kconfig, {"CONFIG_UBSAN", "CONFIG_UBSAN_TRAP", "CONFIG_UBSAN_BOUNDS", "CONFIG_UBSAN_SHIFT"});
        if (err >= 0)
            return 1;
    }
    return 0;
}

// Tries to patch the given blocking bug
int attempt_patch_blocking_bug(const Environment &env, const std::string &bb)
{
    // TODO: find a backport for this bug
    // try to remove a config related to the bug
    if (remove_related_config(env, bb) > 0)
        return 1;
        
    return 0;
}
