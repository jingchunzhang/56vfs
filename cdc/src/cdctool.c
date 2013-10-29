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
#define DISRATIO 20   /*Ä¬ÈÏÌÔÌ­±ÈÀý 20%*/  /*unused + empty*/
#define MAX_TRUST 32
#define IPMODE 0x1F
uint32_t cleanip[MAX_TRUST][256];

typedef struct {
	uint32_t hit;
	uint32_t index;
} t_hitinfo;

enum {CDC_LRU = 0, CDC_LFU};
enum {CDC_STATISTIC = 0, CDC_STATISTIC_ADV, CDC_DISCARD, CDC_FIND, CDC_CLEAN_IP, CDC_CLEAN_IP_FILE, CDC_STATISTIC_TIME};

static void Usage(char **argv)
{
	fprintf(stdout, "Usage %s\n\
			-t s(statistic) S(statistic_adv) d(discard) c(clean_ip_info) e(clean_ip_file_info)\n\
			-T r(lru) f(lfu)\n\
			-q srcdomain:absfile\n\
			-f iplistfile\n\
			-r 20(1-99) discard ratio\n", basename(argv[0]));	
}

static void add_clean_ip(uint32_t ip)
{
	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (cleanip[index][i] == 0)
		{
			cleanip[index][i] = ip;
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
		add_clean_ip(str2ip(s));
		s = e;
	}
	add_clean_ip(str2ip(s));
}

static int do_init_ip(char *ipfile)
{
	memset(cleanip, 0, sizeof(cleanip));
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

static int check_clean_ip(uint32_t ip)
{
	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (cleanip[index][i] == 0)
			return -1;
		if (cleanip[index][i] == ip)
			return 0;
		i++;
	}
	return -1;
}

static void do_sub_clean(t_cdc_val *v)
{
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (v->ip[i] == 0)
			return ;
		if (check_clean_ip(v->ip[i]))
			continue;
		int s;
		get_n_s(i, &s, &(v->status_bists));
		char sip[16] = {0x0};
		char stime[16] = {0x0};
		ip2str(sip, v->ip[i]);
		get_strtime_by_t(stime, v->mtime[i]);
		fprintf(stdout, "%s %s %s\n", sip, stime, s_ip_status[s]); 
		v->ip[i] = 0;
	}
}

static int do_clean_ip()
{
	t_cdc_shmhead *head;
	get_shm_baseinfo(&head);
	uint32_t max = head->hashsize + head->usedsize;
	t_cdc_data *d;
	uint32_t index = 0;
	for(; index < max; index++)
	{
		if (get_index_node(index, &d))
		{
			fprintf(stderr, "get_index_node err %u\n", index);
			break;
		}
		if (d->k.hash1 == 0 && d->k.hash2 == 0 && d->k.hash3 == 0)
			continue;
		t_cdc_val *v = &(d->v);
		do_sub_clean(v);
	}
	return 0;
}

static int do_statistic()
{
	t_cdc_shmhead *head;
	get_shm_baseinfo(&head);
	fprintf(stdout, "TOTAL SIZE %ld, hash size %u data size %u used size %d\n", head->shmsize, head->hashsize, head->datasize, head->usedsize);
	uint32_t max = head->hashsize + head->usedsize;
	t_cdc_data *data;
	uint32_t dvalid = 0;
	uint32_t hvalid = 0;
	uint32_t index = 0;
	for(; index < max; index++)
	{
		if (get_index_node(index, &data))
		{
			fprintf(stderr, "get_index_node err %u\n", index);
			break;
		}
		if (data->k.hash1 == 0 && data->k.hash2 == 0 && data->k.hash3 == 0)
			continue;
		if (index < head->hashsize)
			hvalid++;
		else
			dvalid++;
	}
	fprintf(stdout, "hvalid %u dvalid %u\n", hvalid, dvalid);
	return 0;
}

static int do_statistic_adv()
{
	t_cdc_shmhead *head;
	get_shm_baseinfo(&head);
	uint32_t max = head->hashsize + head->usedsize;
	uint32_t fmd5 = 0;
	uint32_t fmtime = 0;
	uint32_t fall = 0;
	uint32_t all = 0;
	t_cdc_data *data;
	uint32_t index = 0;
	for(; index < max; index++)
	{
		if (get_index_node(index, &data))
		{
			fprintf(stderr, "get_index_node err %u\n", index);
			break;
		}
		if (data->k.hash1 == 0 && data->k.hash2 == 0 && data->k.hash3 == 0)
			continue;

		t_cdc_val *v = &(data->v);
		int f = 0;
		if (strlen(v->fmd5) > 30)
		{
			f = 1 ;
			fmd5++;
		}
		if (v->fmtime > 0 || v->frtime > 0 || v->fctime > 0)
		{
			fmtime++;
			if (f)
				fall++;
		}
		all++;
	}
	fprintf(stdout, "fmd5:%u\nfmtime:%u\nfall:%u\nall:%u\n", fmd5, fmtime, fall, all);

	uint32_t valid = 0;
	for(index = 0; index < head->hashsize; index++)
	{
		uint32_t tmpindex = index;
		while (1)
		{
			if (get_index_node(tmpindex, &data))
			{
				fprintf(stderr, "get_index_node err %u\n", tmpindex);
				break;
			}
			tmpindex = data->next;
			if (data->k.hash1 || data->k.hash2 || data->k.hash3)
				valid++;
			if (data->next == 0)
				break;
		}
	}
	fprintf(stdout, "valid:%u\n", valid);
	return 0;
}

static int sorthit(const void *p1, const void *p2)
{
	t_hitinfo* h1 = (t_hitinfo *) p1;
	t_hitinfo* h2 = (t_hitinfo *) p2;
	return h1->hit > h2->hit;
}

static int do_discard(int type, int ratio)
{
	t_cdc_shmhead *head;
	get_shm_baseinfo(&head);
	uint32_t total = head->hashsize + head->datasize;
	uint32_t max = head->hashsize + head->usedsize;
	if ((total * ratio )/100 <= (total - max))
	{
		fprintf(stdout, "cur shm ok! maxusedsize %u totalsize %u ratio %d\n", max, total, ratio);
		return 0;
	}
	size_t bigsize = sizeof(t_hitinfo) * max;
	t_hitinfo *hitinfo = (t_hitinfo *) malloc (bigsize);
	if (hitinfo == NULL)
	{
		fprintf(stderr, "malloc ERR %ld %m\n", bigsize);
		return -1;
	}
	memset(hitinfo, 0, bigsize);
	t_hitinfo *p = hitinfo;

	t_cdc_data *data;
	uint32_t valid = 0;
	uint32_t index = 0;
	for(; index < max; index++)
	{
		if (get_index_node(index, &data))
		{
			fprintf(stderr, "get_index_node err %u\n", index);
			break;
		}
		if (data->k.hash1 == 0 && data->k.hash2 == 0 && data->k.hash3 == 0)
			continue;
		p->index = index;
		p->hit = data->v.frtime > data->v.fctime ? data->v.frtime : data->v.fctime;
		p->hit = data->v.fmtime > p->hit ? data->v.fmtime : p->hit;
		p++;
		valid++;
	}
	int del = (total * ratio )/100 - (total - valid);
	if (del <= 0)
	{
		fprintf(stdout, "cur shm valid ok! maxusedsize %u totalsize %u ratio %d\n", valid, total, ratio);
		return 0;
	}
	qsort(hitinfo, valid, sizeof(t_hitinfo), sorthit);
	p = hitinfo;
	for(index = 0; index < del; index++)
	{
		clear_index_node(p->index);
		p++;
	}
	fprintf(stdout, "del count %d\n", del);
	return 0;
}

static int do_find(char *s)
{
	t_cdc_data *d;
	if (find_cdc_node(s, &d))
	{
		fprintf(stderr, "find_cdc_node %s no result!\n", s);
		return -1;
	}
	t_cdc_val *v = &(d->v);
	fprintf(stdout, "md5 = %.32s\n", v->fmd5);
	fprintf(stdout, "fmtime = %u\n", v->fmtime);
	fprintf(stdout, "fctime = %u\n", v->fctime);
	fprintf(stdout, "frtime = %u\n", v->frtime);
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (v->ip[i] == 0)
			continue;
		int s;
		get_n_s(i, &s, &(v->status_bists));
		char sip[16] = {0x0};
		char stime[16] = {0x0};
		ip2str(sip, v->ip[i]);
		get_strtime_by_t(stime, v->mtime[i]);
		fprintf(stdout, "%s %s %s\n", sip, stime, s_ip_status[s]); 
	}
	return 0;
}

static int do_statistic_time(char *stime)
{
	time_t ntime = get_time_t(stime);
	if (ntime == 0)
	{
		fprintf(stderr, "stime err %s  %lu\n", stime, strlen(stime));
		return -1;
	}
	t_cdc_shmhead *head;
	get_shm_baseinfo(&head);
	uint32_t max = head->hashsize + head->usedsize;
	uint32_t fmtime = 0;
	uint32_t fdel = 0;

	t_cdc_data *data;
	uint32_t index = 0;
	for(; index < max; index++)
	{
		if (get_index_node(index, &data))
		{
			fprintf(stderr, "get_index_node err %u\n", index);
			break;
		}
		t_cdc_key *k = &(data->k);
		if (k->hash1 == 0 && k->hash2 == 0 && k->hash3 == 0)
			continue;

		t_cdc_val *v = &(data->v);
		if (ntime > vfs_max(vfs_max(v->fmtime, v->frtime), v->fctime))
		{
			memset(k, 0, sizeof(t_cdc_key));
			fmtime++;
		}

		int used = 0;
		int i = 0;
		for (i = 0; i < MAX_IP_IN_DIR; i++)
		{
			if (v->ip[i] == 0)
				continue;
			int s;
			get_n_s(i, &s, &(v->status_bists));
			if (s != CDC_F_DEL)
			{
				used = 1;
				break;
			}
		}
		if (used == 0)
		{
			fdel++;
			memset(k, 0, sizeof(t_cdc_key));
		}
	}
	fprintf(stdout, "fmtime:%u\nfdel:%u\n", fmtime, fdel);
	return 0;
}

static int do_clean_ip_file(char *infile)
{
	time_t mtime = time(NULL);
	FILE *fp = fopen(infile, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "fopen %s err %m!\n", infile);
		return -1;
	}
	char buf[1024] = {0x0};
	while (fgets(buf, sizeof(buf), fp))
	{
		char *sep = strchr(buf, ' ');
		if (sep == NULL)
		{
			fprintf(stderr, "format err %s", buf);
			memset(buf, 0, sizeof(buf));
			continue;
		}
		char *s = sep + 1;
		*sep = 0x0;
		sep = strrchr(s, '\n');
		if (sep)
			*sep = 0x0;
		uint32_t uip = str2ip(buf);
		t_cdc_data *d;
		if (find_cdc_node(s, &d))
		{
			memset(buf, 0, sizeof(buf));
			continue;
		}

		t_cdc_val *v = &(d->v);
		int i = 0;
		for (i = 0; i < MAX_IP_IN_DIR; i++)
		{
			if (v->ip[i] == 0)
				break;
			if (v->ip[i] == uip)
			{
				uint8_t status = CDC_F_DEL;
				set_n_s(i, status, &(v->status_bists));
				v->mtime[i] = mtime;
				break;
			}
		}
		memset(buf, 0, sizeof(buf));
	}
	fclose(fp);
	return 0;
}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		Usage(argv);
		return -1;
	}
	int oper = -1;
	int distype = CDC_LRU;
	int ratio;
	int c;
	char *ipfile = NULL;
	char *s = NULL;
	char *next_arg = NULL;
	while((c = getopt(argc, argv, "t:T:r:q:f:S:")) != EOF)
	{
		switch(c)
		{
			case 'r':
				ratio = atoi(optarg);
				if (ratio < 1 || ratio > 99)
					ratio = DISRATIO;
				break;

			case 'S':
				oper = CDC_STATISTIC_TIME;
				next_arg = optarg;
				break;

			case 'q':
				oper = CDC_FIND;
				s = optarg;
				if (s == NULL)
				{
					fprintf(stderr, "ERROR format for find\n");
					Usage(argv);
					return -1;
				}
				break;

			case 't':
				if (optarg[0] == 's')
					oper = CDC_STATISTIC;
				else if (optarg[0] == 'S')
					oper = CDC_STATISTIC_ADV;
				else if (optarg[0] == 'd')
					oper = CDC_DISCARD;
				else if (optarg[0] == 'c')
					oper = CDC_CLEAN_IP;
				else if (optarg[0] == 'e')
					oper = CDC_CLEAN_IP_FILE;
				else 
				{
					fprintf(stderr, "ERROR type %s\n", optarg);
					Usage(argv);
					return -1;
				}
				break;

			case 'T':
				if (optarg[0] == 'r')
					distype = CDC_LRU;
				else if (optarg[0] == 'f')
					distype = CDC_LFU;
				else 
				{
					fprintf(stderr, "ERROR distype %s\n", optarg);
					Usage(argv);
					return -1;
				}
				break;

			case 'f':
				ipfile = optarg;
				break;

			default:
				fprintf(stderr, "invalid argument[-%c]\n", c);
				return -1;
		}
	}
	if (oper == -1)
	{
		fprintf(stderr, "must have type -t s [-t d]]\n");
		Usage(argv);
		return -1;
	}
	if (link_cdc_write())
	{
		fprintf(stderr, "init_cdc_hash err %m\n");
		return -1;
	}
	if (oper == CDC_STATISTIC || oper == CDC_FIND || oper == CDC_STATISTIC_ADV || CDC_STATISTIC_TIME == oper)
	{
		if (oper == CDC_STATISTIC)
			return do_statistic();
		else if (oper == CDC_STATISTIC_ADV)
			return do_statistic_adv();
		else if (oper == CDC_FIND)
			return do_find(s);
		else
			return do_statistic_time(next_arg);
	}
	if (CDC_CLEAN_IP == oper)
	{
		if (ipfile == NULL)
		{
			fprintf(stderr, "oper = CDC_CLEAN_IP, must have iplistfile!\n");
			return -1;
		}
		if (do_init_ip(ipfile))
			return -1;
		return do_clean_ip();
	}
	if (CDC_CLEAN_IP_FILE == oper)
	{
		if (ipfile == NULL)
		{
			fprintf(stderr, "oper = CDC_CLEAN_IP_FILE, must have ip_file_info!\n");
			return -1;
		}
		return do_clean_ip_file(ipfile);
	}
	return do_discard(distype, ratio);
}
