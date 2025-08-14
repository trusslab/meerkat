#include <argparse.h>
#include <bisect.h>
#include <environment.h>
#include <file_api.h>
#include <fuzz.h>
#include <my_string.h>
#include <port.h>
#include <syzkaller.h>
#include <vm.h>

#include <iostream>
#include <string>
#include <vector>

#include <unistd.h>

using namespace std;

void print_help()
{
    cout << "Usage: ./bin/runner -c [config] -i [id]\n"
        << "    -i [id]: REQUIRED. The id of the bisector (i.e. 1).\n"
        << "    --config (c) [config.cfg]: [config]: REQUIRED. The config file containing the bug information.\n"
        << endl
        << "This Program assumes there is at least one reproducer and both Linux and Syzkaller are built.\n"
        << endl << flush;
}

int handle_bug_config(Environment &env, const Argparse &args)
{
    int err = 0;
    string filename;
    if (args.is_set("config"))
        filename = args.get_string("config");
    else if (args.is_set('c'))
        filename = args.get_string('c');
    else
    {
        cerr << "Error: No config file given (use --config)\n" << flush;
        return -1;
    }

    err = env.parse_config_file(filename);
    if (err < 0)
        return err;

    return err;
}

int main(int argc, char ** argv)
{
    int err;
    Argparse args;
    Environment env;

    args.expect("hci");
    args.expect(vector<string>({ "help", "config" }));
    args.parse(argc, argv);
    if (args.is_set('h') || args.is_set("help"))
    {
        print_help();
        return 0;
    }

    env.init();

    if (handle_bug_config(env, args) < 0)
    {
        cerr << "Failed to parse config.\n" << flush;
        return -1;
    }

    env.duplicates.push_back(env.name);

    // make wd-runner
    if (!check_file(env.vmwd))
    {
        make_dir(env.vmwd);
    }

    // read the execprog options
    std::string prog;
    if (env.primary_repro.empty())
    {
        std::vector<std::string> progs = list_dir(env.reprodir);
        if (progs.empty())
            return -1;
        prog = progs.front();
    }
    else
    {
        prog = env.primary_repro;
    }
    ProgOpts opts;
    opts.from_prog(prog);

    VM_Config vmc;
    vmc.port = 12000;
    vmc.image_path = env.image;
    vmc.image_key = env.image_key;
    vmc.kernel_path = env.kerneldir;
    vmc.wd_path = env.vmwd;
    // Prep the vms
    VMPool vmpool(env.vmc.numVM, vmc);

    vmpool.debug();
    // Boot the vms
    err = vmpool.boot_and_check_all();
    cout << "Booted " << err << " VMs.\n" << flush;

    // Run the prog
    cout << "Copying..." << std::endl;
    vmpool.copy_all(prog);
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-execprog");
    vmpool.copy_all(env.syzdir + "bin/linux_amd64/syz-executor");
    std::string cmd = "./syz-execprog -executor=./syz-executor " + opts.execopts_string() + " " + split(prog, '/').back();
    cout << "Running: " << cmd << std::endl;
    vmpool.run_all(cmd);

    vmpool.wait_loop(10*60);

    std::vector<std::string> logs = vmpool.to_symbolize();

    cout << "Shutting Down.\n" << flush;
    vmpool.kill_all();

    // symbolize the report if needed
    // ./syzkaller/bin/syz-symbolize -kernel_obj wd-meerkat-5/kernel/ wd-meerkat-5/wd-runner/vm-0.log
    Test_Result res;
    res.found = false;
    res.stable = true;
    res.retry = false;
    for (int i = 0; i < logs.size(); i++)
    {
        res.attempts.push_back(symbolize(env, logs.at(i)));
        log_attempt_result_poc(res.attempts.back(), i, env.duplicates);
        res.found = res.attempts.back().found ? true : res.found;
    }
    log_session_result(res);

    return 0;
}
