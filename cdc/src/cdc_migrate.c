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

#define MAX_TRUST 32
#define IPMODE 0x1F
uint32_t trustip[MAX_TRUST][512];
#define NODECOUNT 32000000
#define NEWNODECOUNT 38560000

typedef struct
{
	char     fmd5[32];
	char     fbname[60];
	uint32_t fctime;
	uint32_t frtime;
	uint32_t frcount; 
	uint32_t ip[MAX_IP_IN_DIR];
	uint32_t mtime[MAX_IP_IN_DIR];
	uint64_t status_bists;
	off_t    fsize;
}t_cdc_val_old;

typedef struct
{
	t_cdc_key k;
	t_cdc_val_old v;
	uint32_t next;
}t_cdc_data_old;

static void add_trust_ip(uint32_t ip)
{
	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (trustip[index][i] == 0)
		{
			trustip[index][i] = ip;
			break;
		}
		i++;
	}
	if (i >= 1024)
		fprintf(stderr, "index %u is full ip %u\n", index, ip);
}

static void do_ip_sub_line(char *s)
{
	char *e = NULL;
	while (1)
	{
		e = strchr(s, ',');
		if (e == NULL)
			break;
		*e = 0x0;
		e++;
		add_trust_ip(str2ip(s));
		s = e;
	}
	add_trust_ip(str2ip(s));
}

static int do_init_ip(char *ipfile)
{
	memset(trustip, 0, sizeof(trustip));
	FILE *fp = fopen(ipfile, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen %s err %m\n", ipfile);
		return -1;
	}

	char buf[2048] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		do_ip_sub_line(buf);
		memset(buf, 0, sizeof(buf));
	}
	fclose(fp);
	return 0;
}

static int check_trust_ip(uint32_t ip)
{
	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (trustip[index][i] == 0)
			return -1;
		if (trustip[index][i] == ip)
			return 0;
		i++;
	}
	return -1;
}

static void do_print_v(t_cdc_val_old *v)
{
	int i = 0;
	while (i < MAX_IP_IN_DIR)
	{
		if (v->ip[i])
		{
			if (check_trust_ip(v->ip[i]))
			{
				fprintf(stdout, "%u\n", v->ip[i]);
				v->ip[i] = 0x0;
			}
		}
		i++;
	}
}

static void convert_v(t_cdc_val *nv, t_cdc_val_old *v)
{
	memcpy(nv->fmd5, v->fmd5, sizeof(v->fmd5));
	nv->fmtime = v->fctime;
	nv->frtime = v->frtime;
	memcpy(nv->ip, v->ip, sizeof(v->ip));
	memcpy(nv->mtime, v->mtime, sizeof(v->mtime));
	nv->status_bists = v->status_bists;
}

int main(int argc, char **argv)
{
	if (argc < 3)
	{
		fprintf(stderr, "Usage cdc_migrate dumpfile ipfile!\n");
		return -1;
	}

	if (do_init_ip(argv[2]))
		return -1;

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

	t_cdc_data_old od;
	t_cdc_key *k = &(od.k);
	t_cdc_val_old *v = &(od.v);
	t_cdc_val nv;
	int i = 0;
	while (i < NODECOUNT)
	{
		int rlen = read(fd, &od, sizeof(od));
		if (rlen != sizeof(od))
		{
			close(fd);
			fprintf(stderr, "%s read err %d %m!\n", argv[1], rlen);
			return -1;
		}
		do_print_v(v);
		convert_v(&nv, v);
		if (add_cdc_node_by_key(k, &nv))
		{
			close(fd);
			fprintf(stderr, "add_cdc_node_by_key err %m\n");
			return -1;
		}
		i++;
	}

	close(fd);
	return 0;
}
