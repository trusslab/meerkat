#include <file_api.h>
#include <exec_api.h>
#include <consts.h>

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

string pwd()
{
    char *buf = new char[BUF_SIZE];
    string cwd(getcwd(buf, BUF_SIZE));
    delete[] buf;
    return cwd;
}

int make_dir(const string &filename)
{
    int err = mkdir(filename.c_str(), 0775);

    if (err < 0)
        cerr << "Error: Directory " << filename << " could not be made\n";

    return (err == 0 ? 0 : -1);
}

int remove_file(const string &filename)
{
    int err = remove(filename.c_str());

    if (err != 0)
        cerr << "Error: Failed to delete file " << filename << ".\n";
    
    return (err == 0 ? 0 : -1);
}

int remove_files_in_dir(const string &dir)
{
    int err = 0;
    for (string file : list_dir(dir))
        err = remove_dir(file);

    return err;
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
    if (!check_file(dir))
    {
        cerr << "Error Trying to read a directory that does not exist: "
             << dir << endl << flush;
        return {};
    }
    vector<string> files;
    for (const auto &file : filesystem::directory_iterator(dir))
        files.push_back(file.path().string());

    return files;
}

bool compare_files(const string &file1, const string &file2)
{
    bool ok1, ok2, ret = true;
    string line1, line2;
    ifstream inf1, inf2;
    inf1.open(file1);
    inf2.open(file2);

    if (!inf1 || !inf2)
    {
        cerr << "Error: Failed to open either " << file1 << " or " << file2 << ".\n";
        return false;
    }

    while (true)
    {
        ok1 = getline(inf1, line1) ? true : false;
        ok2 = getline(inf2, line2) ? true : false;

        if (ok1 && ok2)
        {
            if (line1 != line2)
            {
                ret = false;
                break;
            }
        }
        else if (!ok1 && !ok2)
        {
            break;
        }
        else
        {
            ret = false;
            break;
        }
    }

    inf1.close();
    inf2.close();
    return ret;
}

// Loads each line from file "filename" into the vector "lines"
bool load_file(const std::string &filename, std::vector<std::string> &lines)
{
    std::ifstream inf;

    inf.open(filename);
    if (!inf)
    {
        std::cerr << "Could not open file: " << filename << std::endl << std::flush;
        return false;
    }

    std::string l;
    while (getline(inf, l))
    {
        lines.push_back(l);
    }
    inf.close();
    return true;
}

// writes vector "lines" to the file "filename", adds newlines
bool write_file(const std::string &filename, const std::vector<std::string> &lines)
{
    std::ofstream outf;

    outf.open(filename, std::ofstream::out | std::ofstream::trunc);
    if (!outf)
    {
        std::cerr << "Could not open file: " << filename << std::endl << std::flush;
        return false;
    }

    for (std::string l : lines)
        outf << l << std::endl;

    outf.close();
    return true;
}
