#include <iostream>
#include <string>
#include <vector>
#include <cmath>
using namespace std;

const bool MEAN_PLUS_STANDARD = true;

int main (int argc, char * argv[])
{
    if (argc < 2)
        return -1;

    vector<int> times;
    int time;

    for (int i = 1; i < argc; i++)
        times.push_back(stoi(argv[i]));

    if (MEAN_PLUS_STANDARD)
    {
        // time = mean + 1 * std
        double mean = 0;
        double std = 0;

        for (int n : times)
            mean += n;
        mean = mean / times.size();

        for (int i = 0; i < times.size(); i++)
            std += pow((times.at(i) - mean), 2);

        std = std / times.size();
        std = sqrt(std);

        time = mean + 1 * std;
    }
    else
    {
        // time = max * 1.5
        time = times.at(0);
        for (int n : times)
            time = (n > time ? n : time);
        time = time * 1.5;
    }

    time = (time < 10 ? 10 : time);

    cout << time << endl;
}
