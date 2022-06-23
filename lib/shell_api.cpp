#include <shell_api.h>
#include <exec_api.h>
#include <string.h>
#include <string>
#include <iostream>

using namespace std;

// For the record, this is not my favorite way to do this. But
// I'm the goof who decided C++ was the way to go. now I'm mixing
// all sorts of libraries. At least it's only here where I can
// keep track of it.

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