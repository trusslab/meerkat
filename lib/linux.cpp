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

std::set<std::string> gather_commit_tags(const Environment &env, Git &linux_git, const std::string &commit)
{
    std::string tagfile = env.wd + "linux_release_tags.txt";
    std::set<std::string> releases;
    linux_git.dump_commit_past_tags(commit, tagfile);
    
    std::ifstream inf;
    inf.open(tagfile);
    std::string tag;
    while (getline(inf, tag))
    {
        tag = chomp(tag);
        releases.insert(tag);
        // We don't stop at the earliet tested commit for this one.
        // The older tags still exist in the commit and are useful.
    }
    inf.close();
    remove_file(tagfile);

    tag = linux_git.commit_tag(commit);
    if (releases.count(tag) == 0)
        releases.insert(tag);
    return releases;
}

std::vector<Version> gather_release_versions(const Environment &env, Git &linux_git)
{
    std::string tagfile = env.wd + "linux_release_tags.txt";
    std::vector<Version> releases;
    linux_git.dump_tags(tagfile);
    
    std::ifstream inf;
    inf.open(tagfile);
    std::string tag;
    while (getline(inf, tag))
    {
        tag = chomp(tag);
        releases.push_back(Version(tag, linux_git.get_tag_hash(tag), linux_git.get_tag_date(tag)));
        if (tag == EARLIEST_TESTED_VERSION)
            break;
    }
    inf.close();
    remove_file(tagfile);
    return releases;
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

std::string canonical_title(std::string title)
{
    std::vector<std::string> prefixes = {
        "UPSTREAM:",
        "CHROMIUM:",
        "FROMLIST:",
        "BACKPORT:",
        "FROMGIT:",
        "net-backports:"
    };

    for (std::string pf : prefixes)
    {
        if (starts_with(title, pf))
        {
            title = title.substr(pf.size());
            break;
        }
    }

    return trim_space(title);
}

// Apply the same backports as syz-bisect
// From https://github.com/google/syzkaller/blob/master/pkg/vcs/linux_patches.go#L23
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

int write_config(const Environment &env, Git &linux_git, const Version &linux_version, bool need_kcov = true)
{
    std::vector<std::string> lines;
    if (!load_file(env.kconfig, lines))
    {
        std::cerr << "Failed to read kernel config: " << env.kconfig << "\n" << std::flush;
        return -1;
    }

    std::vector<KConfigChange> changes = {
        // 5.2 has CONFIG_SECURITY_TOMOYO_INSECURE_BUILTIN_SETTING which allows to test tomoyo better.
		// This config also enables CONFIG_SECURITY_TOMOYO_OMIT_USERSPACE_LOADER
		// but we need it disabled to boot older kernels.
		KConfigChange("SECURITY_TOMOYO_OMIT_USERSPACE_LOADER", "", "v5.2"),
		// Kernel is boot broken before 4.15 due to double-free in vudc_probe:
		// https://lkml.org/lkml/2018/9/7/648
		// Fixed by e28fd56ad5273be67d0fae5bedc7e1680e729952.
		KConfigChange("USBIP_VUDC", "", "v4.15"),
		// CONFIG_CAN causes:
		// all runs: crashed: INFO: trying to register non-static key in can_notifier
		// for v4.11..v4.12 and v4.12..v4.13 ranges.
		// Fixed by 74b7b490886852582d986a33443c2ffa50970169.
		KConfigChange("CAN", "", "v4.13"),
		// Setup of network devices is broken before v4.12 with a "WARNING in hsr_get_node".
		// Fixed by 675c8da049fd6556eb2d6cdd745fe812752f07a8.
		KConfigChange("HSR", "", "v4.12"),
		// Setup of network devices is broken before v4.12 with a "WARNING: ODEBUG bug in __sk_destruct"
		// coming from smc_release.
		KConfigChange("SMC", "", "v4.12"),
		// Kernel is boot broken before 4.10 with a lockdep warning in vhci_hcd_probe.
		KConfigChange("USBIP_VHCI_HCD", "", "v4.10"),
		KConfigChange("BT_HCIVHCI", "", "v4.10"),
		// Setup of network devices is broken before v4.7 with a deadlock involving team.
		KConfigChange("NET_TEAM", "", "v4.7"),
		// Setup of network devices is broken before v4.5 with a warning in batadv_tvlv_container_remove.
		KConfigChange("BATMAN_ADV", "", "v4.5"),
		// UBSAN is broken in multiple ways before v5.3, see:
		// https://github.com/google/syzkaller/issues/1523#issuecomment-696514105
		KConfigChange("UBSAN", "", "v5.3"),
		// First, we disable coverage in pkg/bisect because it fails machine testing starting from 4.7.
		// Second, at 6689da155bdcd17abfe4d3a8b1e245d9ed4b5f2c CONFIG_KCOV selects CONFIG_GCC_PLUGIN_SANCOV
		// (why?), which is build broken for hundreds of revisions.
		// However, as there's a chance that KCOV might positively affect bug reproduction rate, let's
		// keep it for newer kernel revisions. Bisection algorithm will try to drop it anyway during
		// kernel config minimization.
		KConfigChange("KCOV", "", "v5.4"),
		// This helps to produce stable binaries in presence of kernel tag changes.
		KConfigChange("LOCALVERSION_AUTO", "", "always"),
		// BTF fails lots of builds with:
		// pahole version v1.9 is too old, need at least v1.13
		// Failed to generate BTF for vmlinux. Try to disable CONFIG_DEBUG_INFO_BTF.
		KConfigChange("DEBUG_INFO_BTF", "", "always"),
		// This config only adds debug output. It should not be enabled at all,
		// but it was accidentially enabled on some instances for some periods of time,
		// and kernel is boot-broken for prolonged ranges of commits with deadlock
		// which makes bisections take weeks.
		KConfigChange("DEBUG_KOBJECT", "", "always"),
		// This config is causing problems to kernel signature calculation as new initramfs is generated
		// as a part of every build. Due to this init.data section containing this generated initramfs
		// is differing between builds causing signture being random number.
		KConfigChange("BLK_DEV_INITRD", "", "always"),
        // Even though ORC unwinder was introduced a long time ago, it might have been broken for
		// some time. 5.4 is chosen as a version tag, where ORC unwinder seems to work properly.
		KConfigChange("UNWINDER_ORC", "UNWINDER_FRAME_POINTER", "v5.4")
    };

    // TODO: functionize this
    bool need_ubsan = false;
    if (env.name.find("UBSAN") != std::string::npos)
        need_ubsan = true;
    for (std::string d : env.duplicates)
        if (d.find("UBSAN") != std::string::npos)
            need_ubsan = true;

    std::set<std::string> tags = gather_commit_tags(env, linux_git, linux_version.id);
    for (KConfigChange config : changes)
    {
        if (config.disable == "KCOV" && need_kcov)
            continue;
        
        if (config.disable == "UBSAN" && need_ubsan)
            continue;

        if (config.version == "always" || tags.count(config.version) == 0)
        {
            if (!config.disable.empty())
            {
                std::cout << "CONFIG: Disabling CONFIG_" << config.disable << "\n" << std::flush;
                unset_config("CONFIG_" + config.disable, lines);
            }
            
            if (!config.enable.empty())
            {
                std::cout << "CONFIG: Enabling CONFIG_" << config.enable << "\n" << std::flush;
                set_config("CONFIG_" + config.enable, lines);
            }
        }
    }

    if (!write_file(env.kerneldir + ".config", lines))
    {
        std::cerr << "Failed to write kernel config.\n" << std::flush;
        return -1;
    }

    return 0;
}

void apply_reproducible_build(const Environment &env)
{
    // From Documentation/kbuild/reproducible-builds.rst

    // Give the kernel a randstruct seed
    if (check_file(env.kerneldir + "scripts/gcc-plugins"))
        write_file(env.kerneldir + "scripts/gcc-plugins/randomize_layout_seed.h", {"const char *randstruct_seed = \"e9db0ca5181da2eedb76eba144df7aba4b7f9359040ee58409765f2bdc4cb3b8\";"});

    // Give the kernel a signing key
    if (check_file(env.kerneldir + "certs"))
    {
        write_file(env.kerneldir + "certs/signing_key.pem", {
            "-----BEGIN PRIVATE KEY-----",
            "MIIBVAIBADANBgkqhkiG9w0BAQEFAASCAT4wggE6AgEAAkEAxu5GRXw7d13xTLlZ",
            "GT1y63U4Firk3WjXapTgf9radlfzpqheFr5HWO8f11U/euZQWXDzi+Bsq+6s/2lJ",
            "AU9XWQIDAQABAkB24ZxTGBv9iMGURUvOvp83wRRkgvvEqUva4N+M6MAXagav3GRi",
            "K/gl3htzQVe+PLGDfbIkstPJUvI2izL8ZWmBAiEA/P72IitEYE4NQj4dPcYglEYT",
            "Hbh2ydGYFbYxvG19DTECIQDJSvg7NdAaZNd9faE5UIAcLF35k988m9hSqBjtz0tC",
            "qQIgGOJC901mJkrHBxLw8ViBb9QMoUm5dVRGLyyCa9QhDqECIQCQGLX4lP5DVrsY",
            "X43BnMoI4Q3o8x1Uou/JxAIMg1+J+QIgamNCPBLeP8Ce38HtPcm8BXmhPKkpCXdn",
            "uUf4bYtfSSw=",
            "-----END PRIVATE KEY-----",
            "-----BEGIN CERTIFICATE-----",
            "MIIBvzCCAWmgAwIBAgIUKoM7Idv4nw571nWDgYFpw6I29u0wDQYJKoZIhvcNAQEF",
            "BQAwLjEsMCoGA1UEAwwjQnVpbGQgdGltZSBhdXRvZ2VuZXJhdGVkIGtlcm5lbCBr",
            "ZXkwIBcNMjAxMDA4MTAzMzIwWhgPMjEyMDA5MTQxMDMzMjBaMC4xLDAqBgNVBAMM",
            "I0J1aWxkIHRpbWUgYXV0b2dlbmVyYXRlZCBrZXJuZWwga2V5MFwwDQYJKoZIhvcN",
            "AQEBBQADSwAwSAJBAMbuRkV8O3dd8Uy5WRk9cut1OBYq5N1o12qU4H/a2nZX86ao",
            "Xha+R1jvH9dVP3rmUFlw84vgbKvurP9pSQFPV1kCAwEAAaNdMFswDAYDVR0TAQH/",
            "BAIwADALBgNVHQ8EBAMCB4AwHQYDVR0OBBYEFPhQx4etmYw5auCJwIO5QP8Kmrt3",
            "MB8GA1UdIwQYMBaAFPhQx4etmYw5auCJwIO5QP8Kmrt3MA0GCSqGSIb3DQEBBQUA",
            "A0EAK5moCH39eLLn98pBzSm3MXrHpLtOWuu2p696fg/ZjiUmRSdHK3yoRONxMHLJ",
            "1nL9cAjWPantqCm5eoyhj7V7gg==",
            "-----END CERTIFICATE-----"
        });
    }
}

int build_kernel(const Environment &env, Git &linux_git, const Version &linux_version, const std::string &compiler, bool bisecting, bool need_kcov)
{
    int err = 0;
    std::string old_dir = pwd();

    // for note, Syzkaller uses distclean, oldconfig, and bzimage for make. I have been using clean + git clean, olddefconfig, and "".
    // It also has some KBUILD variables set

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

    clean_kernel(env);
    
    if (!env.feats.no_patch_kernel)
        apply_backports(env, linux_git, linux_version);

    write_config(env, linux_git, linux_version, need_kcov);

    apply_reproducible_build(env);

    // Handle Patches
    if (env.feats.obselete_patches)
        patch_kernel(env, linux_version);

    // build the kernel
    cd(env.kerneldir);
    err = make(env.makeprocs, {"olddefconfig", "ARCH=x86_64", "CC="+compiler}, env.kbuildlog());
    if (err < 0)
        return err;
    err = make(env.makeprocs, {"bzImage", "ARCH=x86_64", "CC="+compiler}, env.kbuildlog());
    if (err < 0)
    {
        std::cerr << "Error: The kernel failed to make.\n";
        cd(old_dir);
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
    err = make(1, std::vector<std::string>({"distclean", "ARCH=x86_64"}), env.kbuildlog());
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
