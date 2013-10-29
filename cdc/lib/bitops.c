#include <stdio.h>
#include <stdint.h>
#include "bitops.h"

void set_n_s(int n, int s, uint64_t *a)
{
	int i = s&0x02;
	int j = s&0x01;

	int c = 0;
	if (n)
		c = n << 1;
	if (i)
		set_bit(c, a);
	else
		clear_bit(c, a);

	if (j)
		set_bit(c+1, a);
	else
		clear_bit(c+1, a);
}

void get_n_s(int n, int *s, uint64_t *a)
{
	*s = 0;

	int c = 0;
	if (n)
		c = n << 1;
	if(test_bit(c, a))
		*s += 2;
	if(test_bit(c+1, a))
		*s += 1;
}

