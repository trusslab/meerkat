#include <stdio.h>
#include <stdlib.h>

// 28 feb
// 30 apr jun nov sept
// 31 jan mar may jul aug oct dec

int main(int argc, char *argv[])
{
	int y = atoi(argv[1]);
    int m = atoi(argv[2]);
    int d = atoi(argv[3]);

	d -= 1;
	if (d < 1)
	{
		m -= 1;
		if (m < 1)
		{
			y -= 1;
			m = 12;
		}
		switch(m)
		{
		case 1:
		case 3:
		case 5:
		case 7:
		case 8:
		case 10:
		case 12:
			d = 31;
			break;
		case 4:
		case 6:
		case 9:
		case 11:
			d = 30;
			break;
		case 2:
			d = (y % 4 == 0) ? 29 : 28;
			break;
		default:
			break;
		}
	}

    char* m0 = m < 10 ? "0" : "";
    char* d0 = d < 10 ? "0" : "";

	printf("%d-%s%d-%s%d", y, m0, m, d0, d);

	return 0;
}
