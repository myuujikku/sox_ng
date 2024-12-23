/*
 * samples.c: Given a list of decimal values,
 * output 16-bit binary little-endian sample values for them.
 */

#include <stdlib.h>
#include <stdio.h>

void
main(int argc, char **argv)
{
    int i;

    for (i=1; i<argc; i++) {
	int val = atoi(argv[i]);
	putchar(val & 0xFF);
	putchar((val >> 8) & 0xFF);
    }
    exit(0);
}
