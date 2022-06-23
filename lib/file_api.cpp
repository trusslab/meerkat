#include <file_api.h>

#include <string>
#include <iostream>
#include <sys/stat.h>
#include <stdio.h>
using namespace std;

bool check_file(const string &filename)
{
    struct stat buf;
    return stat(filename.c_str(), &buf) == 0;
}

int make_dir(const string &filename)
{
    int ret = mkdir(filename.c_str(), 0775);

    if (ret < 0)
        cerr << "Error: Directory " << filename << " could not be made\n";

    return ret;
}

int remove_file(const string &filename)
{
    int ret = remove(filename.c_str());

    if (ret != 0)
        cerr << "Error: Failed to delete file " << filename << ".\n";
    
    return ret;
}