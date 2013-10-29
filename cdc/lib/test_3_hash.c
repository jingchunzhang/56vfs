#include <stdio.h>
#include <stdint.h>
#include "GeneralHashFunctions.h"

int main(int c, char **v)
{
	if (c < 2)
		return 0;
	uint32_t h1, h2, h3;

	get_3_hash(v[1], &h1, &h2, &h3);

	fprintf(stdout, "%u %u %u\n", h1, h2, h3);
	return 0;
}
