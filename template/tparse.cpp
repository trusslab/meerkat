#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cctype>
#include "structs.h"
using namespace std;

const bool VERB = false;

const vector<string> KEYWORDS = {"in", "out", "int64", "int32", "int16", "int8", "intptr", "len", "opt", "timespec", "filename", "array", "const", "ptr64", "string", "flags", "bytesize", "stringnoz", "parent", "inout", "sizeof", "timeout", "ptr", "bool8", "bool16", "bool32", "bool64", "fmt", "void", "int16be", "int8be", "int32be", "int64be", "glob", "bitsize", "offsetof", "vma", "vma64", "proc", "text", "bytesize4", "bytesize8", "bytesize16", "bytesize32", "bytesize64", "dec", "oct", "hex", "align", "packed", "size", "varlen"};

// returns true if s is in v, false otherwise
bool is_in(const string & s, const vector<string> & v)
{
    for (string st : v)
        if (st == s)
            return true;

    return false;
}

// returns true if s is in v, false otherwise
bool is_in(const string & s, const vector<ParseType> & v)
{
    for (ParseType pt : v)
        if (pt.name == s)
            return true;

    return false;
}

// returns the index of the syscall, or -1 on fail
int find(const string & s, const vector<Syscall> & v)
{
    for (int i = 0; i < v.size(); i++)
        if (s == v.at(i).name)
            return i;

    return -1;
}

// returns the index of the item, or -1 on fail
int find(const string & s, const vector<ParseType> & v)
{
    for (int i = 0; i < v.size(); i++)
        if (s == v.at(i).name)
            return i;

    return -1;
}

// returns true if d is in number form, false otherwise
bool is_number(const string &d)
{
    return !d.empty() && (isdigit(d.at(0)) || (d.at(0) == '-' && isdigit(d.at(1))));
}

// returns true if d is NOT in the given vector, false otherwise
bool check_dep(const string &d, const vector<string> &other = vector<string>())
{
    if (!d.empty() && (d.at(0) == '"' || d.at(d.size() - 1) == '"'))
        return false;

    if (!d.empty() && d.at(0) == '\'' && d.at(d.size() - 1) == '\'')
        return false;

    for (string s : other)
        if (d == s)
            return false;

    return !d.empty();
}

void parse_syscall(const string &text, vector<string> &d, vector<string> &args, vector<string> &r)
{
    int pos0 = text.find("(") + 1;
    string tempArg, dep;

    for (int i = pos0; i < text.size() && text.at(i) != ')'; i++)
    {
        // first is always a parameter name
        for (i = (text.at(i) == ' ' ? i + 1 : i); i < text.size() && text.at(i) != ' ' && text.at(i) != ')'; i++);
        if (text.at(i) == ')')
            break;

        tempArg.clear();
        tempArg = text.substr(pos0, i - pos0);
        tempArg = tempArg.at(0) == ' ' ? tempArg.substr(1) : tempArg;
        args.push_back(tempArg);
        pos0 = i + 1;

        // then the value
        for (i++; i < text.size() && text.at(i) != ',' && text.at(i) != '[' && text.at(i) != ')'; i++);
        dep.clear();
        dep = text.substr(pos0, i - pos0);
        if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && !is_number(dep))
            d.push_back(dep);
        pos0 = i + 1;

        if (text.at(i) == ')')
        { // closed paren is the end of the syscall
            break;
        }
        else if (text.at(i) == ',')
        { // comma is the end of the parameter value
            continue;
        }
        else if(text.at(i) == '[')
        { // open bracket signifies more to parse
            int p = 1;
            while (p > 0)
            {
                for(i++; i < text.size() && text.at(i) != ',' && text.at(i) != '[' && text.at(i) != ']'; i++);
                dep.clear();
                dep = text.substr(pos0, i - pos0);

                if (dep.find(":") != string::npos)
                {
                    if (check_dep(dep, KEYWORDS) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                        d.push_back(dep.substr(0, dep.find(":")));

                    if (check_dep(dep, KEYWORDS) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                        d.push_back(dep.substr(dep.find(":") + 1));
                }
                else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && !is_number(dep))
                    d.push_back(dep);

                if (text.at(i) == ',')
                    i++;
                else if (text.at(i) == '[')
                    p++;
                else if (text.at(i) == ']')
                    for (; i < text.size() && text.at(i) == ']'; i++)
                        p--;

                pos0 = i + 1;
            }
        }

        if (text.at(i) == ')')
            break;
    }

    // find any output arguments
    // out is a substring of inout. works
    int pos1 = text.find("out, ");
    while (pos1 != string::npos)
    {
        // it looks like each syscall has at most 1 output parameter.
        // [out, resource]
        pos1 += 5;
        int i;
        for (i = pos1; i < text.size() && text.at(i) != '[' && text.at(i) != ']'; i++);

        dep = text.substr(pos1, i - pos1);
        if (check_dep(dep, KEYWORDS) && !is_number(dep))
            r.push_back(dep);

        // open bracket signifies more to grab
        if (text.at(i) == '[')
        {
            pos1 = i + 1;
            int layers = 1;
            while (layers > 0 && i < text.size())
            {
                // find the next substring
                for (i = pos1; i < text.size() && text.at(i) != '[' && text.at(i) != ']' && text.at(i) != ','; i++);

                dep = text.substr(pos1, i - pos1);
                if (check_dep(dep, KEYWORDS) && !is_number(dep))
                    r.push_back(dep);

                pos1 = i + 1;

                if (text.at(i) == ']')
                    layers--;
                else if (text.at(i) == '[')
                    layers++;
                else if (text.at(i) == ',')
                    pos1 += 1;
            }
        }

        // find the next output parameter
        pos1 = text.find("out, ", pos1);
    }

    // get return type
    pos0 = text.find(")") + 2;
    if (pos0 > text.size() || text.at(pos0) == '(')
        return;

    int pos2 = text.find("(", pos0);

    r.push_back(text.substr(pos0, pos2 - pos0 - 1));

    // remember to clean out args after averything is built
    return;
}

// takes in a single line type and parses it for dependencies
void parse_single_line_type(const string & text, vector<string> &d, const vector<string> &args = vector<string>())
{
    if (text.substr(0, 5) != "type ")
        return;

    int i = 0;
    string dep;

    // find the start of the data portion
    for (i = 5; i < text.size() && text.at(i) != ' ' && text.at(i) != '['; i++);
    if (text.at(i) == '[')
    {
        for (; i < text.size() && text.at(i) != ']'; i++);
        i++;
    }
    int pos0 = i + 1;

    for (i = pos0; i < text.size(); i++)
    {
        for (; i < text.size() && text.at(i) != ' ' && text.at(i) != '['; i++);
        dep.clear();
        dep = text.substr(pos0, i - pos0);
        if (dep.find(":") != string::npos)
        {
            if (check_dep(dep, args) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                d.push_back(dep.substr(0, dep.find(":")));

            if (check_dep(dep, args) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                d.push_back(dep.substr(dep.find(":") + 1));
        }
        else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && check_dep(dep, args) && !is_number(dep))
            d.push_back(dep);
        pos0 = i + 1;

        if (i < text.size() && text.at(i) == '[')
        { // open bracket signifies more to parse
            int p = 1;
            while (p > 0)
            {
                for(i = pos0; i < text.size() && text.at(i) != ',' && text.at(i) != '[' && text.at(i) != ']'; i++)
                {
                    if (text.substr(i, 3) == "\',\'")
                    {
                        i += 2;
                        pos0 = i + 1;
                    }
                }

                dep.clear();
                dep = text.substr(pos0, i - pos0);
                if (!dep.empty())
                    dep = dep.at(0) == ' ' ? dep.substr(1) : dep;

                if (dep.find(":") != string::npos)
                {
                    if (check_dep(dep, args) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                        d.push_back(dep.substr(0, dep.find(":")));

                    if (check_dep(dep, args) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                        d.push_back(dep.substr(dep.find(":") + 1));
                }
                else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && check_dep(dep, args) && !is_number(dep))
                    d.push_back(dep);

                if (text.at(i) == ',')
                    i++;
                else if (text.at(i) == '[')
                    p++;
                else if (text.at(i) == ']')
                    for (; i < text.size() && text.at(i) == ']'; i++)
                        p--;

                pos0 = i + 1;
            }
        }
    }

    return; 
}

// takes in a type line, parses the args if there are any
void parse_type_args(const string & text, vector<string> &args)
{
    if (text.substr(0, 5) != "type ")
        return;

    int i = 0;
    for (i = 5; i < text.size() && text.at(i) != '[' && text.at(i) != ' '; i++);
    if (i >= text.size() || text.at(i) == ' ')
        return;

    int pos0 = i + 1;

    for (i = pos0; i < text.size() && text.at(i) != ']'; i++)
    {
        for (; i < text.size() && text.at(i) != ']' && text.at(i) != ','; i++);
        args.push_back(text.substr(pos0, i - pos0));
        i = (text.at(i) == ',' ? i + 1 : i);
        pos0 = i + 1;

        if (text.at(i) == ']')
            break;
    }
}

void parse_body(const string &text, vector<string> &d, vector<string> &params, const vector<string> &args = vector<string>())
{
    string dep;
    int pos0, i;

    pos0 = text.find("\n") - 1;
    char delim = text.at(pos0) == '{' ? '}' : ']';
    pos0 = text.find("\n") + 1;

    for (i = pos0; i < text.size() && text.at(i) != delim; i++)
    {
        // first the arg name
        for (; i < text.size() && (text.at(i) == '\t' || text.at(i) == ' '); i++);
        pos0 = i;
        for (i = pos0; i < text.size() && text.at(i) != '\t' && text.at(i) != ' '; i++);
        params.push_back(text.substr(pos0, i - pos0));

        for (; i < text.size() && (text.at(i) == '\t' || text.at(i) == ' '); i++);
        pos0 = i;

        // then the value
        for (; i < text.size() && text.at(i) != '[' && text.at(i) != '\n' && text.at(i) != '\t' && text.at(i) != ' '; i++);

        dep.clear();
        dep = text.substr(pos0, i - pos0);
        if (dep.find(":") != string::npos)
        {
            if (check_dep(dep, args) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                d.push_back(dep.substr(0, dep.find(":")));

            if (check_dep(dep, args) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                d.push_back(dep.substr(dep.find(":") + 1));
        }
        else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && check_dep(dep, args) && !is_number(dep))
            d.push_back(dep);
        pos0 = i + 1;

        if(text.at(i) == '[')
        { // open bracket signifies more to parse
            int p = 1;
            while (p > 0)
            {
                for(i = pos0; i < text.size() && text.at(i) != ',' && text.at(i) != '[' && text.at(i) != ']'; i++)
                {
                    if (text.substr(i, 3) == "\',\'")
                    {
                        i += 2;
                        pos0 = i + 1;
                    }
                }

                dep.clear();
                dep = text.substr(pos0, i - pos0);
                if (!dep.empty())
                    dep = dep.at(0) == ' ' ? dep.substr(1) : dep;

                if (dep.find(":") != string::npos)
                {
                    if (check_dep(dep, args) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                        d.push_back(dep.substr(0, dep.find(":")));

                    if (check_dep(dep, args) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                        d.push_back(dep.substr(dep.find(":") + 1));
                }
                else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && check_dep(dep, args) && !is_number(dep))
                    d.push_back(dep);

                if (text.at(i) == ',')
                    i++;
                else if (text.at(i) == '[')
                    p++;
                else if (text.at(i) == ']')
                    for (; i < text.size() && text.at(i) == ']'; i++)
                        p--;

                pos0 = i + 1;
            }
        }

        if (text.at(i) != '\n')
            i = text.find("\n", pos0);
    }

    // something hanging off the end?
    if (text.find("[", i) != string::npos)
    {
        pos0 = text.find("[", i) + 1;
        i = pos0;

        int p = 1;
        while (p > 0)
        {
            for(i = pos0; i < text.size() && text.at(i) != ',' && text.at(i) != '[' && text.at(i) != ']'; i++);
            dep.clear();
            dep = text.substr(pos0, i - pos0);
            if (!dep.empty())
                dep = dep.at(0) == ' ' ? dep.substr(1) : dep;

            if (dep.find(":") != string::npos)
            {
                if (check_dep(dep) && check_dep(dep.substr(0, dep.find(":")), KEYWORDS) && !is_number(dep.substr(0, dep.find(":"))))
                    d.push_back(dep.substr(0, dep.find(":")));

                if (check_dep(dep) && check_dep(dep.substr(dep.find(":") + 1), KEYWORDS) && !is_number(dep.substr(dep.find(":") + 1)))
                    d.push_back(dep.substr(dep.find(":") + 1));
            }
            else if (check_dep(dep, KEYWORDS) && check_dep(dep, d) && !is_number(dep))
                d.push_back(dep);

            if (text.at(i) == ',')
                i++;
            else if (text.at(i) == '[')
                p++;
            else if (text.at(i) == ']')
                for (; i < text.size() && text.at(i) == ']'; i++)
                    p--;

            pos0 = i + 1;
        }
    }

    return;
}

void parse_resource(const string &text, vector<string> &d)
{
    int pos0 = text.find("[") + 1;
    int pos1 = text.find("]");

    if (check_dep(text.substr(pos0, pos1 - pos0), KEYWORDS))
        d.push_back(text.substr(pos0, pos1 - pos0));

    if (text.find(":") != string::npos)
    {
        pos0 = pos1 + 3;
        for (int i = pos0; i < text.size(); i++)
        {
            for (; i < text.size() && text.at(i) != ','; i++);
            if (!is_number(text.substr(pos0, i - pos0)))
                d.push_back(text.substr(pos0, i - pos0));
            i++;
            pos0 = i + 1;
        }
    }

    return;
}

// returns true if this is a constant, false otherwise
bool check_flag(const string & s)
{
    for (char c : s)
        if (!isupper(c) && c != '_')
            return false;

    return true;
}

void parse_flag(const string &text, vector<string> &d)
{
    int pos0 = text.find(" = ") + 3;

    for (int i = pos0; i < text.size(); i++)
    {
        for (; i < text.size() && text.at(i) != ','; i++);
        if (check_flag(text.substr(pos0, i - pos0)))
            d.push_back(text.substr(pos0, i - pos0));
        i++;
        pos0 = i + 1;
    }

    return;
}

// main takes in a repro filename and a list of files in the template
// ./tparse repro.prog `ls full/*.txt`
// argv = "funcall" "repro.prog" "outfile" "file1.txt" "file2.txt" ...
//         0         1            2         3           4
int main(int argc, char *argv[])
{
    // if there aren't enough args, fail.
    if (argc < 4)
        return 1;

    int pos0, pos1, pos2;

    string reproFile(argv[1]);
    vector<string> templateFiles;

    // push all the template files to the vector
    for (int i = 3; i < argc; i++)
        templateFiles.push_back(string(argv[i]));

    fstream templateIn;
    fstream reproIn;
    string line, line2;

    vector<string> includes;
    vector<ParseType> items;
    vector<Syscall> syscalls;
    vector<string> depends;
    vector<string> returns;

    // Time to parse the template
    for (string filename : templateFiles)
    {
        // if a file ever fails to open, fail.
        templateIn.open(filename, fstream::in);
        if (!templateIn)
        {
            cout << "Failed to open file: " << filename << endl;
            return -1;
        }
        
        if (VERB)
            cout << "Parsing " << filename << endl;

        // parse the file line by line
        line.clear();
        while (getline(templateIn, line))
        {
            if (line.empty() || line.at(0) == '#' || line.at(0) == '_')
            { // skip empty lines, commments, and unnamed vars
                line.clear();
                continue;
            }
            else if (line.substr(0, 7) == "include")
            { // if it is an include
                if (!is_in(line.substr(line.find("<")), includes))
                    includes.push_back(line.substr(line.find("<")));
                if (VERB)
                    cout << "include: " << line.substr(line.find("<")) << endl;
            }
            else if (line.substr(0, 8) == "resource")
            { // if it is a resource
                items.push_back(ParseType(line.substr(9, line.find("[", 10) - 9), 'R', line));
                parse_resource(line, items.at(items.size() - 1).depend);
                if (VERB)
                    cout << "resource: " << line.substr(9, line.find("[", 10) - 9) << endl;
            }
            else if (line.substr(0, 4) == "type")
            { // if it is a type
                if (VERB)
                    cout << "type: ";

                if (line.at(line.size() - 1) == '{' || line.at(line.size() - 1) == '[')
                { // check for multi-line type
                    char delimStart = line.at(line.size() - 1);
                    char delimEnd = (delimStart == '{') ? '}' : ']';

                    string name = line.substr(5, line.find("[", 6) - 5);
                    string text = line + "\n";
                    if (VERB)
                        cout << name << ' ' << delimStart;

                    line2.clear();
                    while(getline(templateIn, line2))
                    {
                        if (line2.empty() || line2.at(0) == '#')
                        {
                            line2.clear();
                            continue;
                        }

                        text = text + line2 + "\n";

                        if (line2.at(0) == delimEnd)
                            break;

                        if (VERB)
                            cout << '.';
                        line2.clear();
                    }
                    if (VERB)
                        cout << delimEnd << endl;

                    items.push_back(ParseType(name, 'T', text));
                    vector<string> tempargs;
                    parse_type_args(line, tempargs);
                    parse_body(text, items.at(items.size() - 1).depend, items.at(items.size() - 1).args, tempargs);
                }
                else
                {
                    items.push_back(ParseType(line.substr(5, (line.find(" ", 6) < line.find("[", 6) ? line.find(" ", 6) : line.find("[", 6)) - 5), 'T', line));
                    vector<string> tempargs;
                    parse_type_args(line, tempargs);
                    parse_single_line_type(line, items.at(items.size() - 1).depend, tempargs);
                    if (VERB)
                        cout << line.substr(5, (line.find(" ", 6) < line.find("[", 6) ? line.find(" ", 6) : line.find("[", 6)) - 5) << endl;
                }
            }
            else if (line.substr(0, 6) == "define")
            {
                // this may run into some issues where the define is the sizeof some struct. such depends are not tracked yet.
                // I'll have to deal with it if it becomes an issue.
                // Only an issue if the size of the struct is used, but the struct is not. should never happen.
                items.push_back(ParseType(line.substr(7, (line.find(" ", 8) < line.find("\t", 8) ? line.find(" ", 8) : line.find("\t", 8)) - 7), 'D', line));
                if (VERB)
                    cout << "define: " << line.substr(7, (line.find(" ", 8) < line.find("\t", 8) ? line.find(" ", 8) : line.find("\t", 8)) - 7) << endl;
            }
            else if (line.substr(0, 6) == "incdir")
            {
                if (!is_in(line.substr(line.find("<")), includes))
                    includes.push_back(line.substr(line.find("<")));
                if (VERB)
                    cout << "incdir: " << line.substr(line.find("<")) << endl;
            }
            else if (line.find("(") != string::npos)
            { // if it is a syscall
                syscalls.push_back(Syscall(line.substr(0, line.find("(")), 's', line));
                // parse the line for dependencies and return value
                parse_syscall(line, syscalls.at(syscalls.size() - 1).depend, syscalls.at(syscalls.size() - 1).args, syscalls.at(syscalls.size() - 1).returnType);
                if (VERB)
                    cout << "syscall: " << line.substr(0, line.find("(")) << " (...)" << endl << flush;
            }
            else if (line.at(line.size() - 1) == '{' || line.at(line.size() - 1) == '[')
            { // if it is a struct/union
                char delimStart = line.at(line.size() - 1);
                char delimEnd = (delimStart == '{') ? '}' : ']';

                string name = line.substr(0, line.find(delimStart) - 1);
                string text = line + "\n";
                if (VERB)
                    cout << "struct: " << name << ' ' << delimStart;

                line2.clear();
                while(getline(templateIn, line2))
                {
                    if (line2.empty() || line2.at(0) == '#')
                    {
                        line2.clear();
                        continue;
                    }

                    text = text + line2 + "\n";

                    if (line2.at(0) == delimEnd)
                        break;

                    if (VERB)
                        cout << '.';
                    line2.clear();
                }
                items.push_back(ParseType(name, 'S', text));
                parse_body(text, items.at(items.size() - 1).depend, items.at(items.size() - 1).args);
                if (VERB)
                    cout << delimEnd << endl;
            }
            else if (line.find(" = ") != string::npos)
            { // if it is a flag
                items.push_back(ParseType(line.substr(0, line.find(" = ")), 'F', line));
                parse_flag(line, items.at(items.size() - 1).depend);
                if (VERB)
                    cout << "flag: " << line.substr(0, line.find(" = ")) << endl;
            }
            else {
                cout << "Unknown line type! " << line << endl;
            }

            line.clear();
        }

        templateIn.close();
    }

    // parse the reproducer for a list of syscalls
    reproIn.open(string(argv[1]));
    if(!reproIn)
    {
        cout << "Failed to open reproducer file!" << string(argv[1]) << endl;
        return -1;
    }

    // Always include a few syscalls that are needed for syzkaller to function.
    vector<string> reprosys = {"syz_execute_func", "mmap", "open"};

    while (getline(reproIn, line))
    {
        if (line.empty() || line.at(0) == '#')
            continue;

        pos0 = line.find(" = ");
        pos0 = (pos0 == string::npos) ? 0 : pos0 + 3;
        pos1 = line.find("(");

        reprosys.push_back(line.substr(pos0, pos1 - pos0));
    }
    reproIn.close();

    // for each syscall, add it and all dependencies to depends
    for (string s : reprosys)
    {
        pos2 = find(s, syscalls);
        if (pos2 < 0)
        {
            cout << "Unknown Syscall: " << s << endl;
            continue;
        }

        // no duplicate syscalls
        if (is_in(syscalls.at(pos2).name, depends))
            continue;

        depends.push_back(syscalls.at(pos2).name);

        for (string s : syscalls.at(pos2).depend)
        {
            if ((is_in(s, items) || !is_in(s, syscalls.at(pos2).args)) && !is_in(s, depends))
                depends.push_back(s);
        }

        for (string ret : syscalls.at(pos2).returnType)
        {
            if (!is_in(ret, depends))
            {
                returns.push_back(ret);
                depends.push_back(ret);
            }
        }
    }

    // for each return type, find a syscall that depends on it.
    for (int i = 0; i < returns.size(); i++)
    {
        int syspos = -1;
        for (int j = 0; j < syscalls.size(); j++)
        {
            if (is_in(returns.at(i), syscalls.at(j).depend))
            {
                // keep track of the best syscall (empty return type, fewest dependencies)
                if (is_in(syscalls.at(j).name, depends))
                {
                    syspos = -2;
                    break;
                }
                else if (syscalls.at(j).returnType.empty() && syscalls.at(j).depend.size() == 1)
                {
                    // this is the best syscall
                    syspos = j;
                    break;
                }
                else
                {
                    // keep track of the syscall with the fewest dependencies
                    syspos = (syspos < 0) ? j : syspos;
                    syspos = (syscalls.at(j).depend.size() < syscalls.at(syspos).depend.size()) ? j : syspos;
                    syspos = (syscalls.at(j).depend.size() <= syscalls.at(syspos).depend.size() && syscalls.at(j).returnType.empty()) ? j : syspos;
                }
            }
        }
        // add the chosen syscall
        if (syspos >= 0 && !is_in(syscalls.at(syspos).name, depends))
        {
            depends.push_back(syscalls.at(syspos).name);

            // add each of the syscalls dependencies
            for (string s : syscalls.at(syspos).depend)
                if ((is_in(s, items) || !is_in(s, syscalls.at(syspos).args)) && !is_in(s, depends))
                    depends.push_back(s);

            // add the return type to the end of the list
            for (string ret : syscalls.at(syspos).returnType)
            {
                if (!is_in(ret, depends))
                {
                    returns.push_back(ret);
                    returns.push_back(ret);
                }
            }
        }
    }

    // for each depend, fetch it, and add its depends to the list
    for (int i = 0; i < depends.size(); i++)
    {
        pos2 = find(depends.at(i), items);
        if (pos2 < 0)
            continue;

        for (string s : items.at(pos2).depend)
        {
            if ((is_in(s, items) || !is_in(s, items.at(pos2).args)) && !is_in(s, depends))
                depends.push_back(s);
        }

        // Make sure each resource can be created by a syscall
        if (items.at(pos2).type == 'R')
        {
            int syspos = -1, pos3;
            cout << items.at(pos2).name << endl;

            for (int j = 0; j < depends.size() && syspos != -2; j++)
            {
                syspos = (pos3 = find(depends.at(j), syscalls)) >= 0 && is_in(items.at(pos2).name, syscalls.at(pos3).returnType) ? -2 : syspos;
            }

            for (int j = 0; j < syscalls.size() && syspos != -2; j++)
            {
                if (is_in(items.at(pos2).name, syscalls.at(j).returnType))
                {
                    if (syscalls.at(j).depend.empty())
                    {
                        // if we find a syscall with no dependencies, use it
                        syspos = j;
                        break;
                    }
                    else
                    {
                        // find the syscall with the least amount of dependencies
                        syspos = (syspos < 0) ? j : syspos;
                        syspos = (syscalls.at(j).depend.size() < syscalls.at(syspos).depend.size()) ? j : syspos;
                    }
                }
            }
            // add the chosen syscall
            cout << syspos << endl;
            if (syspos >= 0 && !is_in(syscalls.at(syspos).name, depends))
            {
                cout << syscalls.at(syspos).name << " -> " << items.at(pos2).name << endl;
                depends.push_back(syscalls.at(syspos).name);
                for (string s : syscalls.at(syspos).depend)
                {
                    if ((is_in(s, items) || !is_in(s, syscalls.at(syspos).args)) && !is_in(s, depends))
                        depends.push_back(s);
                }
            }
        }
    }

    // output each depends
    fstream outfile;
    outfile.open(string(argv[2]), fstream::out);
    if (!outfile)
    {
        cout << "Failed to open output file: " << string(argv[2]) << endl;
        return -1;
    }

    for (string s : includes)
        outfile << "include " << s << endl;

    outfile << endl;

    for (string s : depends)
    {
        pos2 = find(s, items);
        if (pos2 >= 0)
        {
            outfile << items.at(pos2).text << endl;
        }

        pos2 = find(s, syscalls);
        if (pos2 >= 0)
        {
            outfile << syscalls.at(pos2).text << endl << endl;
        }
    }

    return 0;
}
