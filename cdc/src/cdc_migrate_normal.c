#include "cdc_hash.h"
#include "common.h"
#include "util.h"
#include "bitops.h"
#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>

#define NEWNODECOUNT 38560000

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		fprintf(stderr, "Usage cdc_migrate dumpfile !\n");
		return -1;
	}

	if (link_cdc_create(NEWNODECOUNT))
	{
		fprintf(stderr, "link_cdc_create err %m\n");
		return -1;
	}

	int fd;
	if ((fd = open64(argv[1], O_RDONLY)) < 0)
	{
		fprintf(stderr, "open %s err %m!\n", argv[1]);
		return -1;
	}

	off_t ret = lseek(fd, sizeof(t_cdc_shmhead), SEEK_CUR);
	if (ret < 0)
	{
		fprintf(stderr, "%s lseek err %lu %m!\n", argv[1], sizeof(t_cdc_shmhead));
		return -1;
	}

	t_cdc_data d;
	t_cdc_key *k = &(d.k);
	t_cdc_val *v = &(d.v);
	int i = 0;
	while (i < NEWNODECOUNT)
	{
		i++;
		int rlen = read(fd, &d, sizeof(d));
		if (rlen != sizeof(d))
		{
			close(fd);
			fprintf(stderr, "%s read err %d %m!\n", argv[1], rlen);
			return -1;
		}
		if (k->hash1 == 0 && k->hash2 == 0 && k->hash3 == 0)
			continue;
		if (add_cdc_node_by_key(k, v))
		{
			close(fd);
			fprintf(stderr, "add_cdc_node_by_key err %m\n");
			return -1;
		}
	}

	close(fd);
	return 0;
}
