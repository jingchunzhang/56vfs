/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

static list_head_t ip_use[256];
static list_head_t ipusehome;
static list_head_t ipusetime;

#define MAX_TASK_COUNT 1000000

#define MAX_SRC_ONCE 1500 /*单个源头机器，24小时内处理的最多任务数*/

#define MIN_SRC_ONCE 256    /*单个源头机器，24小时内处理的任务数低于此值时，不考虑选择逻辑，直接分配任务*/

/*选源逻辑：从配置文件获取当前被推送IP的源头运营商的优先级；
 * 由高到低，依次挑选恰当的IP
 * 当当前优先级的ISP的所有IP的任务数都高于MAX_SRC_ONCE时，开始跳到下个优先级挑选
 */

typedef struct {
	list_head_t tlist;
	list_head_t hlist;
	uint32_t ip;
	uint32_t t;
} t_use_ip_list;

typedef struct {
	uint32_t ip;
	uint32_t u;
} t_use_ip;

static t_use_ip used_ip[32][1024];

static int init_select_ip()
{
	memset(used_ip, 0, sizeof(used_ip));
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&ip_use[i]);
	}
	INIT_LIST_HEAD(&ipusehome);
	INIT_LIST_HEAD(&ipusetime);

	t_use_ip_list *ip0 = (t_use_ip_list*) malloc(sizeof(t_use_ip_list) * MAX_TASK_COUNT);
	if (ip0 == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %s %m\n", FUNC);
		return -1;
	}
	memset(ip0, 0, sizeof(t_use_ip_list) * MAX_TASK_COUNT);
	t_use_ip_list *ip = ip0;
	for (i = 0; i < MAX_TASK_COUNT; i++)
	{
		INIT_LIST_HEAD(&(ip->hlist));
		INIT_LIST_HEAD(&(ip->tlist));
		list_add_head(&(ip->hlist), &ipusehome);
		ip++;
	}
	return 0;
}

static uint32_t get_ip_user_count(uint32_t ip, int type)
{
	t_use_ip * use_ip = used_ip[ip & 0x1F];
	int i = 0;
	for ( i = 0; i < 1024; i++)
	{
		if (use_ip->ip == ip)
		{
			if (type)
			{
				use_ip->u--;
				return 0;
			}
			return use_ip->u;
		}
		if (use_ip->ip == 0)
			return 0;
		use_ip++;
	}
	return 0;
}

static int add_select_ip(uint32_t ip)
{
	t_use_ip_list *useip = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(useip, l, &ipusehome, hlist)
	{
		list_del_init(&(useip->hlist));
		get = 1;
		break;
	}
	if (get == 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "too many run task!\n");
		return -1;
	}
	list_del_init(&(useip->tlist));
	list_head_t *hlist = &(ip_use[ip & 0xFF]);
	useip->ip = ip;
	useip->t = time(NULL);
	list_add_head(&(useip->hlist), hlist);
	list_add_tail(&(useip->tlist), &ipusetime);

	t_use_ip * use_ip = used_ip[ip & 0x1F];
	int i = 0;
	for ( i = 0; i < 1024; i++)
	{
		if (use_ip->ip == ip)
		{
			use_ip->u++;
			return 0;
		}
		if (use_ip->ip == 0)
		{
			use_ip->ip = ip;
			use_ip->u = 1;
			return 0;
		}
		use_ip++;
	}
	return 0;
}

static int select_ip(uint32_t ip[MAXISP][MAX_IP_IN_DIR], char *srcip, t_valid_isp *isp)
{
	int i = 0;
	int j = 0;
	for (i = 0; i < isp->index; i++)
	{
		uint8_t curisp = isp->isp[i];
		uint32_t minip = 0;
		uint32_t mintask = 0;
		for (j = 0; j < MAX_IP_IN_DIR; j++)
		{
			if (ip[curisp][j] == 0)
				break;
			uint32_t curip = ip[curisp][j];
			uint32_t ipcount = get_ip_user_count(curip, 0);
			if (ipcount <= MIN_SRC_ONCE)
			{
				ip2str(srcip, curip);
				LOG(vfs_http_log, LOG_DEBUG, "get src %s\n", srcip);
				return add_select_ip(curip);
			}
			else if (ipcount < MAX_SRC_ONCE)
			{
				if (mintask == 0)
				{
					mintask = ipcount;
					minip = curip;
				}
				else if (ipcount < mintask)
				{
					mintask = ipcount;
					minip = curip;
				}
			}
		}
		if (minip == 0)
			LOG(vfs_http_log, LOG_ERROR, "can not get from %s\n", ispname[curisp]); 
		else
		{
			ip2str(srcip, minip);
			LOG(vfs_http_log, LOG_DEBUG, "get src %s\n", srcip);
			return add_select_ip(minip);
		}
	}
	return -1;
}

static void clear_expire()
{
	list_head_t *l;
	t_use_ip_list *duptask;
	time_t cur = time(NULL);
	list_for_each_entry_safe_l(duptask, l, &ipusetime, tlist)
	{
		if (cur - duptask->t >= 86400)
		{
			list_del_init(&(duptask->hlist));
			list_del_init(&(duptask->tlist));
			list_add_head(&(duptask->hlist), &ipusehome);
			get_ip_user_count(duptask->ip, 1);
		}
		else
			return;
	}
}
