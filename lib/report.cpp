#include <file_api.h>
#include <my_string.h>
#include <report.h>

#include <iostream>
#include <string>
#include <vector>
#include <set>

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

// Ignore some common functions like atomic_read. They are not interesting to the stack trace.
// And often inline which may introduce variance.
bool ignore_functions(const std::string &func)
{
    std::set<std::string> ignore = { "instrument_atomic_read", "atomic_read" };
    return ignore.count(func) > 0;
}

int read_RIP_entries(const std::vector<std::string> &lines, int &i, std::vector<std::string> &stack)
{
    int j = i;
    for (; j < lines.size() && !starts_with(lines.at(j), "RIP: "); j++);
    if (j >= lines.size())
        return 0;

    for (i = j; i < lines.size() && starts_with(lines.at(i), "RIP: "); i++)
    {
        int pos = lines.at(i).find_first_of(":", 5);
        if (pos == std::string::npos)
            return -1;
        std::string func = lines.at(i).substr(pos + 1);
        func = parse_stack_line(func);
        if (!ignore_functions(func))
            stack.push_back(func);
    }

    return 0;
}

int skip_to_call_trace(const std::vector<std::string> &lines, int &i)
{
    for (; i < lines.size() && !starts_with(to_lower(lines.at(i)), "call trace:"); i++);
    if (i >= lines.size() - 1)
    {
        std::cerr << "Error: index error parsing report\n" << std::flush;
        return -1;
    }
    i += lines.at(i + 1).find("<TASK>") != std::string::npos ? 2 : 1;
    return 0;
}

int parse_kasan_stack(const std::vector<std::string> &lines, int &i, std::vector<std::string> &stack)
{
    /* KASAN call trace:
     * BUG: KASAN: use-after-free...
     * ...
     * Call trace:
     *  func1
     *  func2
     *  ...
     */

    std::set<std::string> kasan_functions = { "dump_stack", "__dump_stack", "dump_stack_lvl", "show_stack",
                                "print_address_description", "print_report", "kasan_report", "kasan_tag_mismatch",
                                "__hwasan_tag_mismatch", "__asan_memcpy", "check_region_inline", "kasan_check_range",
                                "__kasan_report" };

    // To start, find the line after "Call trace:". There are no "RIP:"'s in KASAN reports
    if (skip_to_call_trace(lines, i) < 0)
        return -1;
    
    int pos1 = lines.at(i).find_first_not_of(" "), pos2 = 0;

    bool still_kasan = true;
    std::string func;
    for (; i < lines.size(); i++)
    {
        // Look for the end of the stack
        pos2 = lines.at(i).find_first_not_of(" ");
        //std::cout << lines.at(i) << " " << pos1 << " " << pos2 << std::endl << std::flush;
        if (lines.at(i).empty() || lines.at(i).find("</TASK>") != std::string::npos || pos1 != pos2)
        {
            break;
        }

        func = parse_stack_line(lines.at(i));
        // __asan_report_load8_noabort, __asan_report_load1_noabort
        if (still_kasan && (kasan_functions.count(func) > 0 || starts_with(func, "__asan_report_load")))
        {
            stack.clear();
            continue;
        }

        if (!ignore_functions(func))
            stack.push_back(func);
    }

    return 0;
}

int parse_sleeping_stack(const std::vector<std::string> &lines, int &i, std::vector<std::string> &stack)
{
    std::set<std::string> forget_functions = { "dump_stack", "__dump_stack", "dump_stack_lvl", "show_stack",
                                "__might_sleep", "__might_resched", "might_alloc", "kmalloc", "kzalloc",
                                "__mutex_lock", "__mutex_lock_common" };

    // There are no RIP's in these reports (that I have seen)
    if (skip_to_call_trace(lines, i) < 0)
        return -1;

    int pos1 = lines.at(i).find_first_not_of(" "), pos2 = 0;

    std::string func;
    for (; i < lines.size(); i++)
    {
        // Look for the end of the stack
        pos2 = lines.at(i).find_first_not_of(" ");
        if (lines.at(i).empty() || lines.at(i).find("</TASK>") != std::string::npos || pos1 != pos2)
        {
            break;
        }

        // If we find certain functions, forget the stack above.
        // This is a simple way to start the stack after certain functions while not
        // having to be sure I know them all.
        func = parse_stack_line(lines.at(i));
        if (forget_functions.count(func) > 0)
        {
            stack.clear();
            continue;
        }

        if (!ignore_functions(func))
            stack.push_back(func);
    }

    return 0;
}

int parse_warning_stack(const std::vector<std::string> &lines, int &i, std::vector<std::string> &stack)
{
    if (read_RIP_entries(lines, i, stack) < 0)
        return -1;

    // old syzkaller may inject register debugs in the middle of call traces.
    // This will cause issues here.
    if (skip_to_call_trace(lines, i) < 0)
        return -1;

    int pos1 = lines.at(i).find_first_not_of(" "), pos2 = 0;

    std::string func;
    for (; i < lines.size(); i++)
    {
        pos2 = lines.at(i).find_first_not_of(" ");
        if (lines.at(i).empty() || lines.at(i).find("</TASK>") != std::string::npos || pos1 != pos2)
        {
            break;
        }

        func = parse_stack_line(lines.at(i));
        if (!ignore_functions(func))
            stack.push_back(func);
    }

    return 0;
}

// parse one stack trace starting at index i, output to stack vector
int parse_one_stack(Crash_Type ct, const std::vector<std::string> &lines, int &i, std::vector<std::string> &stack)
{
    switch (ct)
    {
    case CT_KASAN:
        return parse_kasan_stack(lines, i, stack);
    case CT_SLEEPING:
        return parse_sleeping_stack(lines, i, stack);
    case CT_WARNING:
        return parse_warning_stack(lines, i, stack);
    case CT_UNKNOWN:
    default:
        return -1;
    }

    return 0;
}

Crash_Type identify_ct(const std::vector<std::string> &lines, int &i)
{
    for (i = 0; i < lines.size(); i++)
    {
        // Start breaking off into identifiable bits
        if (starts_with(lines.at(i), "BUG: "))
        {
            if (lines.at(i).find("KASAN: ") != std::string::npos)
                return CT_KASAN;
            else if (lines.at(i).find("sleeping function called from invalid context") != std::string::npos)
                return CT_SLEEPING;
        }
        else if (starts_with(lines.at(i), "WARNING: "))
        {
            return CT_WARNING;
        }
    }

    return CT_UNKNOWN;
}

// Read a syzkaller-style report file and extract the function names in order of the stack trace.
// On the output of this, the top of stack needs to be useful functions. So cut off any sanitizer
// functions.
int parse_report(const std::string &filename, std::vector<std::string> &stack)
{
    std::vector<std::string> lines;
    if (load_file(filename, lines) < 0)
        return -1;

    int index = 0;
    Crash_Type ct = identify_ct(lines, index);

    // parse the first stack trace from that report
    return parse_one_stack(ct, lines, index, stack);
}
