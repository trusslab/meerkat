#include <file_api.h>

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

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

int remove_dir(const string &dir)
{
    if (!filesystem::remove_all(dir))
    {
        cerr << "Error: Failed to delete directory " << dir << ".\n";
        return -1;
    }

    return 0;
}

int cd(const string &dir)
{
    if (chdir(dir.c_str()) < 0)
    {
        cerr << "Error: Failed to change working directory to " << dir << ".\n";
        return -1;
    }
    return 0;
}

vector<string> list_dir(const string &dir)
{
    vector<string> files;
    for (const auto &file : filesystem::directory_iterator(dir))
        files.push_back(file.path().string());

    return files;
}