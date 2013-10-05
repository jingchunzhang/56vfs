/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

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

