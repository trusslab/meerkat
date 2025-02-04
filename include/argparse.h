#ifndef ARGPARSE_H
#define ARGPARSE_H

#include <vector>
#include <string>
#include <map>
#include <set>

class Argparse
{
private:
    std::string original;                                       // the executed command

    std::string expectTick;                                     // expected flags/ticks
    std::set<std::string> expectLongTick;                       // TODO: Make this a map

    int rawArgCount;
    std::vector<std::string> rawArgVector;                      // stores the args in order exactly as argv would

    std::vector<std::string> badArgs;                           // list of bad arguments

    std::map<char, std::string> tickArgs;                       // parsed args mapped by tick
    std::map<std::string, std::string> longTickArgs;            // parsed args mapped by long tick

public:
    Argparse();                                                 // default constructor
    Argparse(const std::string &);                              // constructor with ticks to expect
    Argparse(const std::vector<std::string> &);                 // constructor with long ticks to expect

    void expect(char);                                          // tells the parser to expect a single tick (adds the tick to the list)
    void expect(const std::string &);                           // tells the parser what ticks to expect (appends)
    void expect(const std::vector<std::string> &);              // tells the parser what long ticks to expect (appends)

    void parse(int, char **);                                   // parses the arguments

    std::string origin() const;                                 // returns the command that was executed

    unsigned int badArg_count() const;                          // returns how many bad arguments were found
    std::vector<std::string> bad_args() const;                  // returns a copy of the bad arguments

    bool is_set(char) const;                                    // checks if a certain tick is set
    bool is_set(const std::string &) const;                     // checks if a certain long tick is set

    std::string get_string(char) const;                  // gets the arg associated with tick and returns as the given type
    std::string get_string(const std::string &) const;   // same but for long ticks

    char get_char(char) const;
    char get_char(const std::string &) const;

    int get_int(char) const;
    int get_int(const std::string &) const;

    void clear_expect();                                        // deletes the expected ticks
    void clear();                                               // deletes all local variables
};

#endif
