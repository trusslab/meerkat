#include <argparse.h>
#include <environment.h>
#include <template_parse.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;

void print_help()
{
    cout << "Usage: ./bin/description_test -c [config] -i [id]\n"
        << "    -i [id]: REQUIRED. The id of the inspector (i.e. 1).\n"
        << "    --config (c) [config.cfg]: [config]: REQUIRED. The config file containing the bug information.\n"
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

    if (handle_bug_config(env, args) < 0)
    {
        cerr << "Failed to parse config.\n" << flush;
        return -1;
    }

    vector<string> syscalls = get_reproduer_syscall_descriptions(env);

    for (string syscall : syscalls)
        cout << syscall << endl << flush;

    return 0;
}