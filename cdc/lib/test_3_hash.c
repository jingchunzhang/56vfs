/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

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
