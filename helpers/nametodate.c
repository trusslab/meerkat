#include <stdio.h>
#include <stdlib.h>

// take in date in the form:
// MMM D, YYYY

int main(int argc, char *argv[])
{
	if (argc < 2)
		return 1;

	char *str = argv[1];
	int i = 0, j = 0;
	char *tok[3];

	// tokenize the string
	tok[0] = str;
	j = 1;
	while(str[i] != '\0' && j < 3)
	{
		if(str[i] == ',')
			str[i] = '\0';

		if(str[i] == ' ')
		{
			tok[j] = str + i + 1;
			str[i] = '\0';
			j++;
		}
		i++;
	}

	// extract the date
	int year = atoi(tok[2]);
	int day = atoi(tok[1]);
	int month = 0;

	switch(tok[0][2])
	{
	case 'n':
		month = (tok[0][1] == 'a') ? 1 : 6;
		break;
    case 'b':
        month = 2;
        break;
    case 'r':
        month = (tok[0][1] == 'a') ? 3 : 4;
        break;
    case 'y':
        month = 5;
        break;
    case 'l':
        month = 7;
        break;
    case 'g':
        month = 8;
        break;
    case 'p':
        month = 9;
        break;
    case 't':
        month = 10;
        break;
    case 'v':
        month = 11;
        break;
    case 'c':
        month = 12;
        break;
	}

	char *d0 = day < 10 ? "0" : "";
	char *m0 = month < 10 ? "0" : "";

	printf("%d-%s%d-%s%d", year, m0, month, d0, day);

	return 0;
}
