#ifndef TEMPLATE_PARSE_H
#define TEMPLATE_PARSE_H

#include <string>
#include <vector>

class ParseType
{
public:
    std::string name;
    char type;          // R T D S F
    std::string text;
    std::vector<std::string> depend;
    std::vector<std::string> args;

    ParseType(const std::string & n, const char t, const std::string & tx)
        : name(n), type(t), text(tx)
    { return; }

    ParseType()
    { return; }
};

class Syscall : public ParseType
{
public:
    std::vector<std::string> returnType;

    Syscall(const std::string & n, const char t, const std::string & tx)
        : ParseType(n, t, tx)
    { return; }

    Syscall()
    { return; }
};

// Takes in a reproducer, an output file, and all of the template .txt files.
// Parses and slims the template such that the outfile file contains just
// enough of the template to recreate the reproducer (or as much of it as 
// possible).
int slim_template(const std::string &, const std::string &, const std::vector<std::string> &);

// creates a vector with a list of all the .txt files in the given template.
std::vector<std::string> list_template_files(const std::string &);

// removes all of the files in the given vector.
int remove_template_files(const std::vector<std::string> &);

bool compare_templates(const std::string &, const std::string &);

#endif