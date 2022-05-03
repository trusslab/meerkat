#include <iostream>
using namespace std;

int daysinmonth(int m, int y)
{
    int t = 30;

    t += 1 * (m == 1 || m == 3 || m == 5 || m == 7 || m == 8 || m == 10 || m == 12);
    t -= 1 * (m == 2) + 1 * (m == 2 && y % 4 != 0);

    return t;
}

// ./dpd yyyy-mm-dd N
int main(int argc, char * argv[])
{
    if (argc != 3)
        return -1;

    // the starting date
    string date = string(argv[1]);
    int y = stoi(date.substr(0, 4));
    int m = stoi(date.substr(5, 2));
    int d = stoi(date.substr(8, 2));

    // numbver of days to add
    int n = stoi(string(argv[2]));

    // add on the number of days
    d += n;

    while (d > daysinmonth(m, y))
    {
        d -= daysinmonth(m, y);
        m++;
        if (m > 12)
        {
            m = 1;
            y++;
        }
    }

    cout << y << "-" << (m < 10 ? "0" : "") << m << "-" << (d < 10 ? "0" : "") << d << endl;

    return 0;
}
