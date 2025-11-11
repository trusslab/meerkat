#include <dedup.h>

#include <iostream>

void usage()
{
    std::cout << "Usage: ./bin/deduplicate bug1/ [bug2/]" << std::flush;
}

int main(int argc, char ** argv)
{
    if (argc <= 1)
    {
        std::cout << "No bugs given.\n" << std::flush;
        usage();
        return 0;
    }

    if (argc > 3)
    {
        std::cout << "Too many arguments.\n" << std::flush;
        usage();
        return 0;
    }

    BugAlias bug1, bug2;
    bug1 = BugAlias((std::string(argv[1])));
    bug1.init();
    std::cout << "Bug 1: " << bug1.debug() << std::endl << std::flush;

    if (argc != 3)
    {
        std::cout << "Nothing to compare against.\n" << std::flush;
        return 0;
    }

    bug2 = BugAlias(std::string(argv[2]));
    bug2.init();
    std::cout << "Bug 2: " << bug2.debug() << std::endl << std::flush;

    // Do the comparison
    std::cout << "Crash Types: " << (compare_crash_types(bug1, bug2) ? "same" : "different") << std::endl << std::flush;
    std::cout << "Crash Names: " << (compare_crash_names(bug1, bug2) ? "same" : "different") << std::endl << std::flush;
    std::cout << "Stack Traces: " << (compare_stack_traces(bug1, bug2) ? "same" : "different") << std::endl << std::flush;

    return 0;
}
