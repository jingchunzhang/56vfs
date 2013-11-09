/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#define DUPHASH 250000
#define DUPSIZE 1000000

static list_head_t dup_use[DUPHASH];
static list_head_t dupusehome;
static list_head_t dupusetime;

typedef struct {
	list_head_t tlist;
	list_head_t hlist;
	uint32_t h1;
	uint32_t h2;
	uint32_t h3;
	uint32_t ip;
	uint32_t t;
} t_check_dup_list;

static int init_check_dup()
{
	int i = 0;
	for (i = 0; i < DUPHASH; i++)
	{
		INIT_LIST_HEAD(&dup_use[i]);
	}
	INIT_LIST_HEAD(&dupusehome);
	INIT_LIST_HEAD(&dupusetime);

	t_check_dup_list *ip0 = (t_check_dup_list*) malloc(sizeof(t_check_dup_list) * DUPSIZE);
	if (ip0 == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %s %m\n", FUNC);
		return -1;
	}
	memset(ip0, 0, sizeof(t_check_dup_list) * DUPSIZE);
	t_check_dup_list *ip = ip0;
	for (i = 0; i < DUPSIZE; i++)
	{
		INIT_LIST_HEAD(&(ip->hlist));
		INIT_LIST_HEAD(&(ip->tlist));
		list_add_head(&(ip->hlist), &dupusehome);
		ip++;
	}
	return 0;
}

static int add_dup_task(char *ip, char *domain, char *fname)
{
	char buf[256] = {0x0};
	snprintf(buf, sizeof(buf), "%s:%s:%s", ip, domain, fname);
	uint32_t h1,h2,h3;
	get_3_hash(buf, &h1, &h2, &h3);
	list_head_t *hashlist = &(dup_use[h1%250000]);
	list_head_t *l;
	t_check_dup_list *duptask;
	list_for_each_entry_safe_l(duptask, l, &dupusehome, hlist)
	{
		list_del_init(&(duptask->hlist));
		duptask->h1 = h1;
		duptask->h2 = h2;
		duptask->h3 = h3;
		duptask->t = time(NULL);

		list_add_head(&(duptask->hlist), hashlist);
		list_add_tail(&(duptask->tlist), &dupusetime);
		return 0;
	}
	LOG(vfs_http_log, LOG_ERROR, "too many task in 24 hours , task %s ignore\n", buf);
	return -1;
}

static int check_dup_task(char *ip, char *domain, char *fname)
{
	char buf[256] = {0x0};
	snprintf(buf, sizeof(buf), "%s:%s:%s", ip, domain, fname);
	uint32_t h1,h2,h3;
	get_3_hash(buf, &h1, &h2, &h3);
	list_head_t *hashlist = &(dup_use[h1%250000]);
	list_head_t *l;
	t_check_dup_list *duptask;
	list_for_each_entry_safe_l(duptask, l, hashlist, hlist)
	{
		if ((h1 == duptask->h1) && (h2 == duptask->h2) && (h3 == duptask->h3))
		{
			LOG(vfs_http_log, LOG_ERROR, "dup task %s\n", buf);
			return 0;
		}
	}
	return -1;
}

static void clear_dup_expire()
{
	list_head_t *l;
	t_check_dup_list *duptask;
	time_t cur = time(NULL);
	list_for_each_entry_safe_l(duptask, l, &dupusetime, tlist)
	{
		if (cur - duptask->t >= 86400)
		{
			list_del_init(&(duptask->hlist));
			list_del_init(&(duptask->tlist));
			list_add_head(&(duptask->hlist), &dupusehome);
		}
		else
			return;
	}
}
