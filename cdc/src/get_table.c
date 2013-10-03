#include <stdio.h>
#include <stdint.h>

uint32_t r5hash(const char *p) 
{
	uint32_t h = 0;
	while(*p) {
		h = h * 11 + (*p<<4) + (*p>>4);
		p++;
	}
	return h;
}

int main(int c, char **v)
{
	if (c != 2)
		return;
	uint32_t index = r5hash(v[1]) & 0x3F;
	fprintf(stdout, "%u\n", index);
	return 0;
}

