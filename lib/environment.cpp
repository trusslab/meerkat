#include <consts.h>
#include <environment.h>
#include <exec_api.h>
#include <file_api.h>
#include <json.h>
#include <my_string.h>
#include <port.h>

#include <string>
#include <vector>
#include <set>
#include <sstream>
#include <fstream>
#include <iostream>
#include <iomanip>

#include <string.h>

int Environment::init()
{
    fuzztimes = DEFAULT_FUZZ_TIMES;

    vmd.numVM = NUM_VM_DEFAULT;
    vmd.numCPU = NUM_CPU_DEFAULT;
    vmd.numProcs = NUM_PROCS_DEFAULT;
    vmr.numVM = NUM_VM_MT;
    vmr.numCPU = NUM_CPU_MT;
    vmr.numProcs = NUM_PROCS_MT;
    vmst.numVM = NUM_VM_ST;
    vmst.numCPU = NUM_CPU_ST;
    vmst.numProcs = NUM_PROCS_ST;
    memory = VM_MEM;
    makeprocs = MAKE_PROCS;

    feats.poc_test = false;
    feats.ff_test = false;
    feats.setup_only = false;
    feats.find_only = false;
    feats.poc_all_pocs = false;
    feats.ff_no_find_backup = false;
    feats.stateful_corpus = false;
    feats.patch_kernel = false;

    return 0;
}

int Environment::parse_config_file(const std::string &filename)
{
    JSON json;
    if (!json.parse(filename))
    {
        std::cerr << "Error: (environment) Failed to parse json file " << filename << "\n" << std::flush;
        return -1;
    }

    if (json.has_name("bug_name") && json.is_type("bug_name", JSON_Val_string))
    {
        name = json.get_string("bug_name");
    }
    else
    {
        std::cerr << "Error: Bug Title was not given (\"bug_name\": \"KASAN: use-after-free ...\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("bug_link") && json.is_type("bug_link", JSON_Val_string))
    {
        buglink = json.get_string("bug_link");
    }

    if (json.has_name("arch") && json.is_type("arch", JSON_Val_string))
    {
        arch = json.get_string("arch");
    }
    else
    {
        std::cerr << "Error: Arch was not given (\"arch\": \"amd64|i386\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("repository") && json.is_type("repository", JSON_Val_string))
    {
        repository = json.get_string("repository");
    }
    else
    {
        std::cerr << "Error: Kernel Repository was not given\n" << std::flush;
        return -1;
    }

    if (json.has_name("branch") && json.is_type("branch", JSON_Val_string))
    {
        branch = json.get_string("branch");
    }
    else
    {
        branch = "master";
    }

    if (json.has_name("kernel_config") && json.is_type("kernel_config", JSON_Val_string))
    {
        kconfig = json.get_string("kernel_config");
        kconfig = starts_with(kconfig, "/") ? kconfig : wd + kconfig;
    }
    else
    {
        std::cerr << "Error: Kernel Config was not given (\"kernel_config\": \"/path/to/config.txt\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("reproducers") && json.is_type("reproducers", JSON_Val_string))
    {
        reprodir = json.get_string("reproducers");
        reprodir = starts_with(reprodir, "/") ? reprodir : wd + reprodir;
        reprodir = ends_with(reprodir, "/") ? reprodir : reprodir + "/";
    }
    else
    {
        std::cerr << "Error: Reproducers Directory was not given (\"reproducers\": \"/path/to/reproducers/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("reproducer") && json.is_type("reproducer", JSON_Val_string))
    {
        primary_repro = json.get_string("reproducer");
        primary_repro = primary_repro.empty() || starts_with(primary_repro, "/") ? primary_repro : reprodir + primary_repro;
    }

    if (json.has_name("bugID") && json.is_type("bugID", JSON_Val_string))
    {
        working_name = json.get_string("bugID");
    }
    else
    {
        working_name.clear();
    }

    if (json.has_name("wd") && json.is_type("wd", JSON_Val_string))
    {
        wd = json.get_string("wd");
        wd = ends_with(wd, "/") ? wd : wd + "/";
    }
    else
    {
        std::cerr << "Error: No working directory was given (\"wd\": /path/to/wd/)\n" << std::flush;
        return -1;
    }

    if (json.has_name("syzconfig") && json.is_type("syzconfig", JSON_Val_string))
    {
        syzconfig = json.get_string("syzconfig");
        syzconfig = starts_with(syzconfig, "/") ? syzconfig : wd + syzconfig;
    }
    else
    {
        syzconfig = wd + "syzkaller.cfg";
    }

    if (json.has_name("home") && json.is_type("home", JSON_Val_string))
    {
        home = json.get_string("home");
    }
    else
    {
        std::cerr << "Error: No home directory was given (\"home\": \"/path/to/" << PROJECT_NAME << "/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("syzkaller") && json.is_type("syzkaller", JSON_Val_string))
    {
        syzdir = json.get_string("syzkaller");
        syzdir = starts_with(syzdir, "/") ? syzdir : home + syzdir;
    }
    else
    {
        std::cerr << "Error: No Syzkaller was given (\"syzkaller\": \"/path/to/syzkaller/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("compilers") && json.is_type("compilers", JSON_Val_string))
    {
        compiler_dir = json.get_string("compilers");
        compiler_dir = starts_with(compiler_dir, "/") ? compiler_dir : home + compiler_dir;
    }
    else
    {
        std::cerr << "Error: No Compiler Directory was given (\"compilers\": \"/path/to/compilers/\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("compiler") && json.is_type("compiler", JSON_Val_string))
    {
        compiler = json.get_string("compiler");
    }
    else
    {
        std::cerr << "Error: No default compiler was given (\"compiler\": \"gcc|clang\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("ccache") && json.is_type("ccache", JSON_Val_string))
    {
        ccache = json.get_string("ccache");
    }
    else
    {
        ccache.clear();
    }

    if (json.has_name("image") && json.is_type("image", JSON_Val_string))
    {
        image = json.get_string("image");
        image = starts_with(image, "/") ? image : home + image;
    }
    else
    {
        std::cerr << "Error: No Image was given (\"image\": \"/path/to/image.img\")\n" << std::flush;
        return -1;
    }

    if (json.has_name("image_key") && json.is_type("image_key", JSON_Val_string))
    {
        image_key = json.get_string("image_key");
        image_key = starts_with(image_key, "/") ? image_key : home + image_key;
    }
    else
    {
        std::cerr << "Error: No Image Key was given (\"image_key\": \"/path/to/image.id_rsa\")\n" << std::flush;
        return -1;
    }

    kerneldir = wd + "kernel/";
    syzwd = wd + "wd-kaller/";
    vmwd = wd + "wd-runner/";
    logdir = wd + "log/";
    syzkaller_log = logdir + (working_name.empty() ? "" : working_name + "-") + "kaller.log";

    return 0;
}

void Environment::default_features()
{
    feats.poc_test = true;
    feats.ff_test = true;
    feats.stateful_corpus = true;
    feats.poc_all_pocs = true;
    return;
}

int Environment::handle_features(const std::set<std::string> &features)
{
    if (features.size() == 0 || features.find("default") != features.end())
    {
        default_features();
        return 0;
    }

    // PoC usage will be determined in manager/by user (?)

    if (features.find("all") != features.end())
    {
        feats.ff_test = true;
        feats.poc_test = true;
        feats.stateful_corpus = true;
        feats.patch_kernel = true;
    }
    
    if (features.find("poc-test") != features.end())
    {
        feats.poc_test = true;
    }

    if (features.find("ff-test") != features.end())
    {
        feats.ff_test = true;
    }

    if (features.find("setup-only") != features.end())
    {
        feats.setup_only = true;
    }

    if (features.find("find-only") != features.end())
    {
        feats.find_only = true;
    }

    if (features.find("poc-all-pocs") != features.end())
    {
        feats.poc_all_pocs = true;
    }

    if (features.find("ff-no-find-backup") != features.end())
    {
        feats.ff_no_find_backup = true;
    }

    if (!feats.ff_test && !feats.poc_test)
    {
        std::cerr << "Error: At least one of poc-test and ff-test must be set.\n" << std::flush;
        return -1;
    }

    if (features.find("stateful-corpus") != features.end())
    {
        if (feats.ff_test)
            feats.stateful_corpus = true;
        else
            std::cerr << "Info: stateful-corpus requires ff-test. Skipping.\n" << std::flush;
        
    }

    if (features.find("patch-kernel") != features.end())
    {
        feats.patch_kernel = true;
    }

    // check that at least one feature is set
    return 0;
}

std::string Environment::syscall_string() const
{
    if (required_syscalls.size() <= 0)
        return "";

    std::stringstream ss;
    ss << "[";
    for (int i = 0; i < required_syscalls.size(); i++)
    {
        ss << "\"" << required_syscalls.at(i) << "\"" << (i < required_syscalls.size() - 1 ? ", " : "");
    }
    ss << "]";
    return ss.str();
}

void Environment::config_print(const std::string &label, const std::string &config, const int n) const
{
    std::cout << std::left << std::setw(CONFW) << (label + ":" + (n != -1 ? " (" + std::to_string(n) + ")" : ""))
              << config << std::endl << std::flush;
}

std::string __version(const std::string &cmd)
{
    char * command = new char[cmd.size() + 1];
    strcpy(command, cmd.c_str());
    char arg1[] = "--version";

    char * arg_list[] = {command, arg1, nullptr};
    std::string ret = exec_and_read(cmd, arg_list);

    ret = split(ret, '\n').at(0);

    delete[] command;
    return ret;
}

void Environment::print() const
{
    config_print("Syzbot Link", buglink);
    if (!working_name.empty())
        config_print("Working Name", working_name);

    if (duplicates.size() > 1)
    {
        std::cout << "\nAliases:\n";
        for (std::string s : duplicates)
            std::cout << "    " << s << std::endl;
    }
    else
        std::cout << "\nNo aliases given.\n";
    
    std::cout << std::endl;
    config_print("Anchor Commit", anchor_hash);
    config_print("Repository", repository);
    config_print("Branch", branch);
    config_print("Arch", arch);
    std::cout << std::endl;
    config_print(PROJECT_NAME, home);
    config_print("Version", REVISION);
    config_print("Kernel", kerneldir);
    config_print("Syzkaller", syzdir);
    config_print("Compilers", compiler_dir);
    config_print("Compiler", __version(compiler));
    config_print("Linker", __version("ld"));
    config_print("Ccache", __version(ccache));
    config_print("Image", image);
    config_print("Image Key", image_key);
    std::cout << std::endl;
    config_print("Workdir", wd);
    config_print("Kconfig", kconfig);
    if (!reprodir.empty())
        config_print("Reproducers", reprodir, list_dir(reprodir).size());
    if (!primary_repro.empty())
        config_print("Primary PoC", primary_repro);
    config_print("Syzkaller Workdir", syzwd);
    config_print("Log Directory", logdir);
    std::cout << std::endl;
    if (required_syscalls.size() > 0)
        config_print("Syscalls", syscall_string(), required_syscalls.size());
    config_print("VM Count", std::to_string(vmc.numVM));
    config_print("CPU Count", std::to_string(vmc.numCPU));
    config_print("Procs", std::to_string(vmc.numProcs));
    config_print("Memory", std::to_string(memory));
    config_print("Make Procs", std::to_string(makeprocs));
    if (feats.ff_test)
    {
        config_print("Max Time", std::to_string(max_time));
        config_print("Max Attempts", std::to_string(fuzztimes));
    }
    

    return;
}
