#include <file_api.h>
#include <my_string.h>
#include <report.h>

#include <string>
#include <vector>

void sanitize_function_name(std::string &function)
{
    int pos;
    std::vector<std::string> transforms = {".part", ".constprop", ".cold", ".isra"};
    for (std::string t : transforms)
    {
        pos = function.find(t);
        if (pos != std::string::npos)
        {
            function = function.substr(0, pos);
            break;
        }
    }
}

// Takes a valid stack line "line", and parses down to just the funciton name
std::string parse_stack_line(std::string line)
{
    int pos;
    bool cont = true;
    std::string ret;

    if (ends_with(line, "[inline]"))
        line = line.substr(0, line.size() - 9);

    line = line.substr(line.find_first_not_of(" "));
    pos = line.find_first_of("+ ", 0);
    if (pos != std::string::npos)
        ret = line.substr(0, pos);
    else
        ret = line;
    sanitize_function_name(ret);
    pos = line.find(" ");
    cont = (pos != std::string::npos);
    line = cont ? line.substr(pos + 1) : "";
    
    return ret;
}

// Read a syzkaller-style report file and extract the function names in order of the stack trace.
int parse_report(const std::string &filename, std::vector<std::string> &stack)
{
    std::vector<std::string> lines;
    if (load_file(filename, lines) < 0)
        return -1;
    
    return 0;
}
