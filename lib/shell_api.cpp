#include <shell_api.h>
#include <exec_api.h>
#include <my_string.h>

#include <string>
#include <vector>
#include <iostream>
#include <cctype>

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

string date(const string &format)
{
    string plus_format = "+" + format;

    char command[] = "date";
    char * arg1 = new char[plus_format.size() + 1];
    strcpy(arg1, plus_format.c_str());

    char * arg_list[] = {command, arg1, nullptr};
    string ret = exec_and_read("date", arg_list);

    delete[] arg1;
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

int wc_l(const string &filename)
{
    char command[] = "wc";
    char arg1[] = "-l";
    char * arg2 = new char[filename.size() + 1];
    strcpy(arg2, filename.c_str());

    char * arg_list[] = {command, arg1, arg2, nullptr};
    string output = exec_and_read("wc", arg_list);

    int ret = 0;
    for (int i = 0; i < output.size() && isdigit(output.at(i)); i++)
        ret = (ret * 10) + (output.at(i) - '0');

    delete[] arg2;
    return ret;
}
