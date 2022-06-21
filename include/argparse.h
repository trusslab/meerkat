#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <vector>
#include <string>
#include <map>

class Argparse
{
private:
    std::string expectTick;                                     // expected flags/ticks
    std::vector<std::string> expectLongTick;

    int rawArgCount;
    std::vector<std::string> rawArgVector;                      // stores the args in order exactly as argv would

    std::map<char, std::string> tickArgs;                       // parsed args mapped by tick
    std::map<std::string, std::string> longTickArgs;            // parsed args mapped by long tick

public:
    Argparse();                                                 // default constructor
    Argparse(const std::string &);                              // constructor with ticks to expect
    Argparse(const std::vector<std::string> &);                 // constructor with long ticks to expect

    void expect(char);                                          // tells the parse to expect a single tick (adds the tick to the list)
    void expect(const std::string &);                           // tells the parser what ticks to expect
    void expect(const std::vector<std::string> &);              // tells the parser what long ticks to expect

    void parse(int, char **);                                   // parses the arguments

    bool is_set(char) const;                                    // checks if a certain tick is set
    bool is_set(const std::string &) const;                     // checks if a certain long tick is set

    std::string get_arg_as_string(char) const;                  // gets the arg associated with tick and returns as the given type
    std::string get_arg_as_string(const std::string &) const;   // same but for long ticks

    char get_arg_as_char(char) const;
    char get_arg_as_char(const std::string &) const;

    int get_arg_as_int(char) const;
    int get_arg_as_int(const std::string &) const;

    void clear_expect();                                        // deletes the expected ticks
    void clear();                                               // deletes all local variables
};

#endif
