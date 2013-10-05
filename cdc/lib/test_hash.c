/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#include "cdc_hash.h"
#include "bitops.h"
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

int main(int argc, char **argv)
{
	if (argc < 2)
		return -1;
	if (link_cdc_read())
	{
		fprintf(stderr, "init_cdc_hash err %m\n");
		return -1;
	}
	fprintf(stdout, "%lu %lu\n", sizeof(t_cdc_shmhead), sizeof(t_cdc_data));

	char *file = argv[1];
	t_cdc_data *d;
	if (find_cdc_node(file, &d))
	{
		fprintf(stderr, "find_cdc_node err %m\n");
		return -1;
	}
	t_cdc_val *v1 = &(d->v);
	int s;
	get_n_s(0, &s, &(v1->status_bists));
	fprintf(stdout, "%u, %u %ld %d \n", v1->ip[0], v1->mtime[0], time(NULL), s);
	return 0;
}
