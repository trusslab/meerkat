#include <date.h>
#include <argparse.h>

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <fstream>

using namespace std;

int main(int argc, char ** argv)
{
    Argparse args;
    args.expect("sefFmipd");
    args.expect(vector<string>({ "setup-only" }));

    args.parse(argc, argv);
}
