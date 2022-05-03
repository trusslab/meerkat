#include <stdio.h>
#include <stdlib.h>

// takes in 3 args year month day, prints decrimented date in form yyyy-mm-dd
int main(int argc, char *argv[])
{
	int y = atoi(argv[1]);
	int m = atoi(argv[2]);
	int d = atoi(argv[3]);
	int dmax = 0;

	switch(m)
	{
    case 1:
    case 3:
    case 5:
    case 7:
    case 8:
    case 10:
    case 12:
		dmax = 31;
		break;
    case 4:
    case 6:
    case 9:
    case 11:
        dmax = 30;
        break;
    case 2:
        dmax = (y % 4 == 0) ? 29 : 28;
        break;
    default:
        break;
	}

	d++;
	if (d > dmax)
	{
		m++;
		d = 1;
		if (m > 12)
		{
			y++;
			m = 1;
		}
	}

	char *m0 = m < 10 ? "0" : "";
	char *d0 = d < 10 ? "0" : "";
	printf("%d-%s%d-%s%d", y, m0, m, d0, d);

	return 0;
}
