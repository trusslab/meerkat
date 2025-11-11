#include <file_api.h>
#include <exec_api.h>
#include <my_string.h>
#include <consts.h>

#include <string>
#include <iostream>
#include <fstream>
#include <filesystem>

#include <sys/stat.h>
#include <stdio.h>
#include <unistd.h>

bool check_file(const std::string &filename)
{
    struct stat buf;
    return stat(filename.c_str(), &buf) == 0;
}

std::string pwd()
{
    char *buf = new char[BUF_SIZE];
    std::string cwd(getcwd(buf, BUF_SIZE));
    delete[] buf;
    return cwd;
}

int make_dir(const std::string &filename)
{
    int err = mkdir(filename.c_str(), 0775);

    if (err < 0)
        std::cerr << "Error: Directory " << filename << " could not be made\n";

    return (err == 0 ? 0 : -1);
}

int remove_file(const std::string &filename)
{
    int err = remove(filename.c_str());

    if (err != 0)
        std::cerr << "Error: Failed to delete file " << filename << ".\n";
    
    return (err == 0 ? 0 : -1);
}

int remove_files_in_dir(const std::string &dir)
{
    int err = 0;
    for (std::string file : list_dir(dir))
        err = remove_dir(file);

    return err;
}

int remove_dir(const std::string &dir)
{
    if (!std::filesystem::remove_all(dir))
    {
        std::cerr << "Error: Failed to delete directory " << dir << ".\n";
        return -1;
    }

    return 0;
}

int cd(const std::string &dir)
{
    if (chdir(dir.c_str()) < 0)
    {
        std::cerr << "Error: Failed to change working directory to " << dir << ".\n";
        return -1;
    }
    return 0;
}

std::vector<std::string> list_dir(const std::string &dir)
{
    if (!check_file(dir))
    {
        std::cerr << "Error Trying to read a directory that does not exist: "
             << dir << std::endl << std::flush;
        return {};
    }
    std::vector<std::string> files;
    for (const auto &file : std::filesystem::directory_iterator(dir))
        files.push_back(file.path().string());

    return files;
}

// Compares 2 files. returns true if they have no differences, false otherwise
bool compare_files(const std::string &file1, const std::string &file2)
{
    bool ok1, ok2, ret = true;
    std::string line1, line2;
    std::ifstream inf1, inf2;
    inf1.open(file1);
    inf2.open(file2);

    if (!inf1 || !inf2)
    {
        std::cerr << "Error: Failed to open either " << file1 << " or " << file2 << ".\n";
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
        // Sanitize return carriages. I guess that's an issue now...
        l = chomp(l);
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
