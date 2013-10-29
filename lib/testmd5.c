/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdio.h>
#include <libgen.h>
#include "util.h"

int main(int argc, char **argv)
{
	if (argc != 2)
	{
		fprintf(stderr, "Usage %s infile!\n", basename(argv[0]));
		return -1;
	}
	unsigned char md5[16] = {0x0};
	unsigned char smd5[36] = {0x0};

	getfilemd5(argv[1], md5);

	unsigned char *s = smd5;
	int l = 0;
	int i = 0;
	for (i = 0; i < 16; i++)
	{
		l += snprintf(s + l, sizeof(smd5) -l , "%02x", md5[i]);
	}
	fprintf(stdout, "%s %s\n", smd5, argv[1]);

	unsigned char tmd5[36] = {0x0};		
	getfilemd5view(argv[1], tmd5);
	fprintf(stdout, "%s %s\n", tmd5, argv[1]);
	return 0;
}
