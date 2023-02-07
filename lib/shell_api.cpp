#include <shell_api.h>
#include <exec_api.h>
#include <inspector_config.h>

#include <string>
#include <iostream>

#include <string.h>
#include <stdlib.h>

using namespace std;

string get_path()
{
    char * path = getenv("PATH");

    if (path == nullptr)
        return "";

    return string(path);
}

int export_env(const string &e)
{
    char * e_c = new char[e.size() + 1];
    strcpy(e_c, e.c_str());
    int err = putenv(e_c);
    if (err < 0)
    {
        cerr << "Error: Could not export " << e << ".\n";
        return err;
    }

    // This might be a memory leak, but environment variables get
    // cranky when I free the stuff they were using. And it's probably
    // best not to free the PATH variable.
    // delete[] e_c;
    return 0;
}

int set_timezone(const string &tz)
{
    string env = "TZ=" + tz;
    return (export_env(env) == 0 ? 0 : -1);
}

// lynx -dump -dont_wrap_pre -width=1000 link
int lynx_dump(const std::string &link, const string &dumpfile)
{
    char command[] = "lynx";
    char arg1[] = "-dump";
    char arg2[] = "-dont_wrap_pre";
    char arg3[] = "-width=1000";
    char * arg4 = new char[link.size() + 1];
    strcpy(arg4, link.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};

    int ret = exec_and_wait("lynx", arg_list, dumpfile);

    delete[] arg4;
    return ret;
}

int sed_i(const string &regex, const string &filename)
{
    char command[] = "sed";
    char arg1[] = "-i";
    char * arg2 = new char[regex.size() + 1];
    char * arg3 = new char[filename.size() + 1];

    strcpy(arg2, regex.c_str());
    strcpy(arg3, filename.c_str());

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int ret = exec_and_wait("sed", arg_list);

    delete[] arg2;
    delete[] arg3;
    return ret;
}

bool grep_to_find(const string &expr, const string &file)
{
    char command[] = "grep";
    char * arg1 = new char[expr.size() + 1];
    strcpy(arg1, expr.c_str());
    char * arg2 = new char[file.size() + 1];
    strcpy(arg2, file.c_str());

    char * arg_list[] = {command, arg1, arg2, nullptr};

    int ret = exec_and_wait("grep", arg_list, "/dev/null");

    delete[] arg1;
    delete[] arg2;
    return ret == 0 ? true : false;
}

int make(int procs, const string &option)
{
    char command[] = "make";

    string j = "-j" + to_string(procs);
    char * arg1 = new char[j.size() + 1];
    strcpy(arg1, j.c_str());

    char arg2[] = "-f";
    char arg3[] = "Makefile";

    char * arg4 = nullptr;
    if (!option.empty())
    {
        arg4 = new char[option.size() + 1];
        strcpy(arg4, option.c_str());
    }

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};

    int err = exec_and_wait("make", arg_list);

    delete[] arg1;
    if (arg4)
        delete[] arg4;

    return (err == 0 ? 0 : -1);
}

int syz_env_cross_compile(const string &syz_env, const Bug_Info &bug)
{
    char command[] = "sudo";
    char * arg1 = new char[syz_env.size() + 1];
    strcpy(arg1, syz_env.c_str());
    char arg2[] = "make";
    char arg3[] = "TARGETVMARCH=amd64";
    char arg4[] = "TARGETARCH=386";

    char * arg_list[] = {command, arg1, arg2, arg3, arg4, nullptr};

    int err = exec_and_wait("sudo", arg_list);

    delete[] arg1;
    return (err != 0 ? -1 : 0);
}

int syz_env_clean(const string &syz_env, const Bug_Info &bug)
{
    char command[] = "sudo";
    char * arg1 = new char[syz_env.size() + 1];
    strcpy(arg1, syz_env.c_str());
    char arg2[] = "make";
    char arg3[] = "clean";

    char * arg_list[] = {command, arg1, arg2, arg3, nullptr};

    int err = exec_and_wait("sudo", arg_list);

    delete[] arg1;
    return (err != 0 ? -1 : 0);
}

int copy(const string &src, const string &dest)
{
    char command[] = "cp";
    char * arg1 = new char[src.size() + 1];
    strcpy(arg1, src.c_str());
    char * arg2 = new char[dest.size() + 1];
    strcpy(arg2, dest.c_str());

    char * arg_list[] = {command, arg1, arg2, nullptr};

    int ret = exec_and_wait("cp", arg_list);

    delete[] arg2;
    delete[] arg1;
    return ret;
}

int move(const string &src, const string &dest)
{
    char command[] = "mv";
    char * arg1 = new char[src.size() + 1];
    strcpy(arg1, src.c_str());
    char * arg2 = new char[dest.size() + 1];
    strcpy(arg2, dest.c_str());

    char * arg_list[] = {command, arg1, arg2, nullptr};

    int ret = exec_and_wait("mv", arg_list);

    delete[] arg2;
    delete[] arg1;
    return ret;
}

int export_go(const InspectorConfig &inspector)
{
    string goroot = "GOROOT=" + inspector.get_go_dir();
    int err = export_env(goroot);
    if (err < 0)
        return err;

    string path = "PATH=" + inspector.get_go_dir() + "/bin:" + get_path();
    err = export_env(path);

    return err;
}

int go_mod_init()
{
    char command[] = "go";
    char arg1[] = "mod";
    char arg2[] = "init";
    char * arg_list[] = {command, arg1, arg2, nullptr};

    return exec_and_wait("go", arg_list);
}

int go_mod_tidy()
{
    char command[] = "go";
    char arg1[] = "mod";
    char arg2[] = "tidy";
    char * arg_list[] = {command, arg1, arg2, nullptr};

    return exec_and_wait("go", arg_list);
}

int go_mod_vendor()
{
    char command[] = "go";
    char arg1[] = "mod";
    char arg2[] = "vendor";
    char * arg_list[] = {command, arg1, arg2, nullptr};

    return exec_and_wait("go", arg_list);
}
