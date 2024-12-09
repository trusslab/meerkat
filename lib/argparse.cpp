#include <argparse.h>

#include <string>
#include <vector>
#include <map>
#include <set>

Argparse::Argparse()
{ return; }

Argparse::Argparse(const std::string & e)
    : expectTick(e)
{ return; }

Argparse::Argparse(const std::vector<std::string> & e)
{
    for (std::string s : e)
        expectLongTick.insert(s);
}

void Argparse::expect(char t)
{
    expectTick += t;
    return;
}

void Argparse::expect(const std::string & e)
{
    expectTick += e;
    return;
}

void Argparse::expect(const std::vector<std::string> & e)
{
    for (std::string s : e)
        expectLongTick.insert(s);

    return;
}

void Argparse::parse(int argc, char ** argv)
{
    if (argc <= 0 || argv == nullptr)
        return;
    
    original = "";
    for (int i = 0; i < argc; i++)
        original += (i == 0 ? "" : " ") + std::string(argv[i]);

    // TODO: stop parsing once we hit a pipe or redirection
    rawArgCount = argc;
    for (int i = 0; i < argc; i++)
        rawArgVector.push_back(std::string(argv[i]));

    // find all of the ticks that we were told to expect
    for (int i = 0; i < rawArgVector.size(); i++)
    {
        if (rawArgVector.at(i).size() > 1 && rawArgVector.at(i).at(0) == '-' && rawArgVector.at(i).at(1) == '-')
        {
            // handle long tick
            std::string a;
            if (i + 1 < rawArgVector.size() && rawArgVector.at(i + 1).size() > 0 && rawArgVector.at(i + 1).at(0) != '-')
                a = rawArgVector.at(i + 1);
            else
                a = "";

            if (expectLongTick.find(rawArgVector.at(i).substr(2)) != expectLongTick.end())
                longTickArgs.insert(std::pair<std::string, std::string>(rawArgVector.at(i).substr(2), a));
            else
                badArgs.push_back(rawArgVector.at(i).substr(2));
        }
        else if (rawArgVector.at(i).size() > 0 && rawArgVector.at(i).at(0) == '-')
        {
            // handle small tick(s)
            int tickCount = rawArgVector.at(i).size() - 1;
            std::string ticks = rawArgVector.at(i).substr(1);

            if (tickCount == 1 && i + 1 < rawArgVector.size()
                && rawArgVector.at(i + 1).size() > 0 && rawArgVector.at(i + 1).at(0) != '-')
            {
                char t = ticks.at(0);
                if (expectTick.find(t) != std::string::npos)
                    tickArgs.insert(std::pair<char, std::string>(t, rawArgVector.at(i + 1)));
                else
                    badArgs.push_back(std::string(1, t));
            }
            else if (tickCount >= 1)
            {
                for (char t : ticks)
                {
                    if (expectTick.find(t) != std::string::npos)
                        tickArgs.insert(std::pair<char, std::string>(t, ""));
                    else
                        badArgs.push_back(std::string(1, t));
                }
            }
        }
    }
}

std::string Argparse::origin() const
{
    return original;
}

unsigned int Argparse::badArg_count() const
{
    return badArgs.size();
}
std::vector<std::string> Argparse::bad_args() const
{
    return badArgs;
}

bool Argparse::is_set(char t) const
{
    return tickArgs.count(t) > 0;
}

bool Argparse::is_set(const std::string & t) const
{
    return longTickArgs.count(t) > 0;
}

std::string Argparse::get_arg_as_string(char t) const
{
    if (!is_set(t))
        return "";

    return tickArgs.at(t);
}

std::string Argparse::get_arg_as_string(const std::string & t) const
{
    if (!is_set(t))
        return "";

    return longTickArgs.at(t);
}

char Argparse::get_arg_as_char(char t) const
{
    if (!is_set(t))
        return '\0';

    return tickArgs.at(t).at(0);
}

char Argparse::get_arg_as_char(const std::string & t) const
{
    if (!is_set(t))
        return '\0';

    return longTickArgs.at(t).at(0);
}

int Argparse::get_arg_as_int(char t) const
{
    if (!is_set(t))
        return -1;

    return std::stoi(tickArgs.at(t));
}

int Argparse::get_arg_as_int(const std::string & t) const
{
    if (!is_set(t))
        return -1;

    return std::stoi(longTickArgs.at(t));
}

void Argparse::clear_expect()
{
    expectTick.clear();
    expectLongTick.clear();
    return;
}

void Argparse::clear()
{
    clear_expect();
    rawArgCount = 0;
    rawArgVector.clear();
    tickArgs.clear();
    longTickArgs.clear();
    return;
}
