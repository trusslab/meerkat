#include <iostream>
#include <string>
using namespace std;

int daystomonth(int m, int y)
{
    int total = 0;

    switch(m)
    {
    case 12:
        total += 30;
    case 11:
        total += 31;
    case 10:
        total += 30;
    case 9:
        total += 31;
    case 8:
        total += 31;
    case 7:
        total += 30;
    case 6:
        total += 31;
    case 5:
        total += 30;
    case 4:
        total += 31;
    case 3:
        total += 28 + (y%4 == 0);
    case 2:
        total += 31;
    case 1:
        break;
    default:
        break;
    }
    return total;
}

// take in 2 dates and prints out their difference
int main(int argc, char *argv[])
{
    if (argc < 3)
        return 1;

    string date1(argv[1]);
    string date2(argv[2]);

    // I sure hope the formatting is good.
    int y1 = stoi(date1.substr(0, 4));
    int m1 = stoi(date1.substr(5, 2));
    int d1 = stoi(date1.substr(8, 2));

    int y2 = stoi(date2.substr(0, 4));
    int m2 = stoi(date2.substr(5, 2));
    int d2 = stoi(date2.substr(8, 2));

    int days1 = y1*365 + (y1 - 1)/4 + daystomonth(m1, y1) + d1;
    int days2 = y2*365 + (y2 - 1)/4 + daystomonth(m2, y2) + d2;

    cout << days1 - days2 << endl;

    return 0;
}
