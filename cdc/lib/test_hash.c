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
