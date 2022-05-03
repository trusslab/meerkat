#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
	if(argc <= 1 || argc > 4)
		return 1;

	int y = atoi(argv[1]);
	int m = atoi(argv[2]);
	int d = atoi(argv[3]);

	char* m0 = m < 10 ? "0" : "";
	char* d0 = d < 10 ? "0" : "";

	printf("%d-%s%d-%s%d", y, m0, m, d0, d);
	return 0;
}
