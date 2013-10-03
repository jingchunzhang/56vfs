#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "GeneralHashFunctions.h"
#include "vfs_so.h"
#include "vfs_init.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "bitops.h"
#include "acl.h"
#include "cdc_hash.h"
#include "cdc_http.h"
int vfs_http_log = -1;

uint32_t g_index = 0;
static char * defualtmd5 = "md5";

#define PRE_MALLOC 1000
extern char *iprole[];

static list_head_t iplist[256];
static list_head_t tasklist[256];
static list_head_t taskhome;
t_path_info cdc_path;
char voss_tmpdir[256] = {0x0};
char voss_indir[256] = {0x0};
char hotfile_prefix[128] = {0x0};

static pthread_mutex_t reload_mutex = PTHREAD_MUTEX_INITIALIZER;
#define IPMODE 0x1F
uint32_t trustip[32][1024];

char *sendbuf = NULL;
#define SENDBUFSIZE 2048000

#include "cdc_http_rule.c"
#include "cdc_http_sub.c"
#include "cdc_http_ip.c"
#include "cdc_http_dup.c"

int fileinfo_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = snprintf(buf, len, "fileinfo=%s#", t);
	t_cdc_data *d;
	if (find_cdc_node(t, &d))
	{
		LOG(vfs_http_log, LOG_DEBUG, "find_cdc_node %s no result!\n", t);
		return l;
	}
	t_cdc_val *v = &(d->v);
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
		if (l >= len)
			return l;
		l += snprintf(buf+l, len -l , "%s %s %s#", sip, stime, s_ip_status[s]); 
	}
    return fileinfo_p_sub(t, buf, l, len);	
}

int ipinfo_p(char *url, char *buf, int len)
{
	char *t = url;
	char subbuf[1024] = {0x0};
	int l = snprintf(buf, len, "ipinfo=%s#", t);
	uint32_t ip = str2ip(t);
	list_head_t *hashlist = &(iplist[ip&0xFF]);
	t_cdc_http_task *task = NULL;
	list_head_t *l0;
	list_for_each_entry_safe_l(task, l0, hashlist, iplist)
	{
		if (task->ip == ip)
		{
			memset(subbuf, 0, sizeof(subbuf));
			char stime[16] = {0x0};
			get_strtime_by_t(stime, task->mtime);
			char stime1[16] = {0x0};
			get_strtime_by_t(stime1, task->starttime);
			int l1 = snprintf(subbuf, sizeof(subbuf), "%s %d %s %s %s %s %s %s %s#", task->file, task->type, t, task->srcip, task->dstip, task->task_status, task->role, stime, stime1);
			if (l + l1 >= len)
			{
				LOG(vfs_http_log, LOG_ERROR, "too long %d %d %d\n", l, l1, len);
				return l;
			}
			l += snprintf(buf+l, len -l , "%s", subbuf);
		}
	}
	return l;
}

int taskinfo_p(char *url, char *buf, int len)
{
	char *t = url;
	char subbuf[1024] = {0x0};
	int l = snprintf(buf, len, "taskinfo=%s#", t);
	list_head_t *hashlist = &(tasklist[r5hash(t)&0xFF]);
	t_cdc_http_task *task = NULL;
	list_head_t *l0;
	list_for_each_entry_safe_l(task, l0, hashlist, tlist)
	{
		if (strcmp(task->file, t) == 0)
		{
			memset(subbuf, 0, sizeof(subbuf));
			char stime[16] = {0x0};
			get_strtime_by_t(stime, task->mtime);
			char stime1[16] = {0x0};
			get_strtime_by_t(stime1, task->starttime);
			char sip[16] = {0x0};
			ip2str(sip, task->ip);
			int l1 = snprintf(subbuf, sizeof(subbuf), "%s %d %s %s %s %s %s %s %s#", task->file, task->type, sip, task->srcip, task->dstip, task->task_status, task->role, stime, stime1);
			if (l + l1 >= len)
			{
				LOG(vfs_http_log, LOG_ERROR, "too long %d %d %d\n", l, l1, len);
				return l;
			}
			l += snprintf(buf+l, len -l , "%s", subbuf);
		}
	}
	return l;
}

#define SYNCFILE_SEP "&iplist="
#define SYNCFILE_SEP_LEN 8

typedef struct {
	char *f;
	char *d;
	char *fc;
} t_sync_file;

typedef struct {
	char ip[16];
	uint8_t isp;
} t_ip_isp;

typedef struct {
	char ip[16];
} t_ips;

static void add_a_valid_ip(uint32_t tip, uint32_t ip[MAX_IP_IN_DIR])
{
	int i = 0; 
	for ( i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (ip[i] == tip)
			return;
		if (ip[i] == 0)
		{
			ip[i] = tip;
			return;
		}
	}
}

static int get_srcip(char *dstip, t_ip_isp * isps, char *srcip, char *f)
{
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	if (get_ip_info(&ipinfo, dstip, 1))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_ip_info err %s\n", dstip);
		return -1;
	}
	uint32_t udstip = str2ip(dstip);

	uint32_t ip[MAXISP][MAX_IP_IN_DIR];
	memset(ip, 0, sizeof(ip));
	t_valid_isp *isp = &(valid_isp[ipinfo->isp%MAXISP]);
	int j = 0;
	for (j = 0; j < MAX_IP_IN_DIR; j++)
	{
		if (strlen(isps->ip) == 0)
			break;
		add_a_valid_ip(str2ip(isps->ip), ip[isps->isp%MAXISP]);
		isps++;
	}

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(f, &cs))
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s \n", f);
	else
	{
		for (j = 0; j < cs.index; j++)
		{
			if (cs.ip[j] == 0)
				break;
			if (udstip == cs.ip[j])
				continue;
			add_a_valid_ip(cs.ip[j], ip[cs.isp[j]%MAXISP]);
		}
	}
	return select_ip(ip, srcip, isp);
}

static void do_check_ip(char *fc, t_ip_isp * isps, char **fmd5)
{
	t_cdc_data *d = NULL;
	if (find_cdc_node(fc, &d))
	{
		LOG(vfs_http_log, LOG_ERROR, "find_cdc_node %s no result, agent will try src!\n", fc);
		*fmd5 = defualtmd5;
		return ;
	}
	t_cdc_val *v = &(d->v);
	*fmd5 = v->fmd5;
	*fmd5 = defualtmd5;
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	int j = 0;
	for (j = 0; j < MAX_IP_IN_DIR; j++)
	{
		memset(isps, 0, sizeof(t_ip_isp));
		if (v->ip[j] == 0)
			continue;
		int s;
		get_n_s(j, &s, &(v->status_bists));
		if (s != CDC_F_OK)
			continue;
		ip2str(isps->ip, v->ip[j]);
		if (get_ip_info(&ipinfo, isps->ip, 1))
			LOG(vfs_http_log, LOG_ERROR, "get_ip_info err in shm %s\n", isps->ip);
		else
		{
			isps->isp = ipinfo->isp;
			isps++;
		}
	}
}

static int get_hot_ips(int type, char *f, t_ips *ips)
{
	if (type == CNC || TEL == type)
	{
		if (get_cfg_lock())
		{
			LOG(vfs_http_log, LOG_ERROR, "get_hot_ips err %d, try later!\n", type);
			return -1; 
		}
		t_ip_info_list *server = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(server, l, &hothome, hotlist)
		{
			if (server->ipinfo.isp == type)
			{
				snprintf(ips->ip, sizeof(ips->ip), "%s", server->ipinfo.sip);
				LOG(vfs_http_log, LOG_NORMAL, "hotfile %s %s\n", ips->ip, ispname[type]);
				ips++;
			}
		}
		release_cfg_lock();
		return 0;
	}

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(f, &cs))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s %m\n", f);
		return -1; 
	}
	int i = 0;
	for (i = 0; i < cs.index; i++)
	{
		if (cs.isp[i] != type)
			continue;
		ip2str(ips->ip, cs.ip[i]);
		LOG(vfs_http_log, LOG_NORMAL, "hotfile %s %s\n", ips->ip, ispname[type]);
		ips++;
	}
	return 0;
}

static int check_self_in_shm(char *s, t_ip_isp *isps, char *f, char *d)
{
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if(strcmp(s, isps->ip) == 0)
		{
			LOG(vfs_http_log, LOG_NORMAL, "%s have %s:%s now!\n", s, d, f);
			return 0;
		}
		isps++;
	}
	if (check_hotip_task(s, f, d) == 0)
	{
		LOG(vfs_http_log, LOG_NORMAL, "%s have %s:%s now in db!\n", s, d, f);
		return 0;
	}
	return -1;
}

static void do_hot_sync(int type, t_sync_file *p, char *o, int *ol)
{
	char srcip[16] = {0x0};
	int l = 0;
	char subbuf[1024];
	t_ips ips[2048];
	memset(ips, 0, sizeof(ips));
	if (get_hot_ips(type, p->f, ips))
	{
		LOG(vfs_http_log, LOG_ERROR, "hotfile %s get_hot_ips err\n", p->fc);
		return;
	}
	t_ip_isp isps[MAX_IP_IN_DIR];
	memset(isps, 0, sizeof(isps));
	char *fmd5 = NULL;
	do_check_ip(p->fc, isps, &fmd5);
	int i = 0;
	for (i = 0; i < 2048; i++)
	{
		if (strlen(ips[i].ip) == 0)
			break;
		if (check_self_in_shm(ips[i].ip, isps, p->f, p->d) == 0)
			continue;
		if (check_dup_task(ips[i].ip, p->d, p->f) == 0)
			continue;
		memset(srcip, 0, sizeof(srcip));
		memset(subbuf, 0, sizeof(subbuf));
		if (type == MP4) 
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s\n", ips[i].ip, p->d, p->f);
		else if (get_srcip(ips[i].ip, isps, srcip, p->f) == 0)
		{
			if (add_dup_task(ips[i].ip, p->d, p->f))
			{
				LOG(vfs_http_log, LOG_ERROR, "add_dup_task err %s\n", p->f);
				return;
			}
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s,%s,%s\n", ips[i].ip, p->d, p->f, srcip, fmd5);
		}
		else
		{
			LOG(vfs_http_log, LOG_ERROR, "%s ip %s get_srcip error\n", p->fc, ips[i].ip);
			continue;
		}
		if (l + *ol >= SENDBUFSIZE)
		{
			LOG(vfs_http_log, LOG_ERROR, "hotfile %s too long ip %d:%d:%d\n", p->fc, l, *ol, SENDBUFSIZE);
			return;
		}
		*ol += snprintf(o + *ol, SENDBUFSIZE - *ol, "%s", subbuf);
	}
}

static void do_syncfile_iplist(char *s, t_sync_file *p, char *o, int *ol)
{
	char srcip[16] = {0x0};
	int l = 0;
	char subbuf[1024];
	t_ip_isp isps[MAX_IP_IN_DIR];
	memset(isps, 0, sizeof(isps));
	char *fmd5 = NULL;
	do_check_ip(p->fc, isps, &fmd5);
	while (1)
	{
		char *t = strchr(s, ',');
		if (t == NULL)
			break;
		*t = 0x0;
		if (check_self_in_shm(s, isps, p->f, p->d) == 0)
			continue;
		if (check_dup_task(s, p->d, p->f) == 0)
			continue;
		memset(srcip, 0, sizeof(srcip));
		memset(subbuf, 0, sizeof(subbuf));
		if (get_srcip(s, isps, srcip, p->f))
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s\n", s, p->d, p->f);
		else
			l = snprintf(subbuf, sizeof(subbuf), "iplist=%s&vfs_cmd=M_SYNCFILE&%s=%s,%s,%s\n", s, p->d, p->f, srcip, fmd5);
		if (add_dup_task(s, p->d, p->f))
		{
			LOG(vfs_http_log, LOG_ERROR, "add_dup_task err %s:%s:%s\n", s, p->d, p->f);
			return;
		}
		if (l + *ol >= SENDBUFSIZE)
		{
			LOG(vfs_http_log, LOG_ERROR, "hotfile %s too long ip %d:%d:%d\n", p->fc, l, *ol, SENDBUFSIZE);
			return;
		}
		*ol += snprintf(o + *ol, SENDBUFSIZE - *ol, "%s", subbuf);
		s = t + 1;
	}
}

static void do_syncfile(int type, t_sync_file *p, char *o, int *ol)
{
	switch(type)
	{
		case ALL:
			do_syncfile(ALLHOT, p, o, ol);
			do_syncfile(YD, p, o, ol);
			do_syncfile(HS, p, o, ol);
			do_syncfile(EDU, p, o, ol);
			do_syncfile(TT, p, o, ol);
			break;

		case ALLHOT:
			do_syncfile(TEL, p, o, ol);
			do_syncfile(CNC, p, o, ol);
			break;

		case CNC:
		case TEL:
		case YD:
		case HS:
		case EDU:
		case TT:
		case MP4:
		default:
			do_hot_sync(type, p, o, ol);
			break;

	}
}

static void put_sync_info_2_voss(char *buf, int l)
{
	LOG(vfs_http_log, LOG_NORMAL, "%s:%d\n", FUNC, LN);
	char fname[256] = {0x0};
	char stime[16] = {0x0};
	get_strtime(stime);

	snprintf(fname, sizeof(fname), "%s/voss_sync_%s.%d", voss_tmpdir, stime, g_index++);
	int fd = open(fname, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (fd < 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", fname);
		return ;
	}

	int wlen = write(fd, buf, l);
	if (l != wlen)
	{
		LOG(vfs_http_log, LOG_ERROR, "open %s err %m\n", fname);
		close(fd);
		return ;
	}
	close(fd);
	char fname2[256] = {0x0};
	snprintf(fname2, sizeof(fname2), "%s/voss_sync_%s.%d", voss_indir, stime, g_index);

	if (rename(fname, fname2))
	{
		LOG(vfs_http_log, LOG_ERROR, "rename %s to %s err %m\n", fname, fname2);
		return ;
	}
}

int hotfile_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = 0; 
	char *p = strstr(t, SYNCFILE_SEP);
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	*p = 0x0;
	char *p1 = p + SYNCFILE_SEP_LEN;
	p = strchr(t, '/');
	if (p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	*p = 0x0;
	t_sync_file pp;
	char domain[64] = {0x0};
	snprintf(domain, sizeof(domain), "%s", t);
	if (strncmp(domain, "fcs", 3) || !strstr(domain, ".56.com"))
	{
		LOG(vfs_http_log, LOG_ERROR, "err format hotfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format hotfile\n");
		return l;
	}
	pp.d = domain;
	p++;
	char fname[256] = {0x0};
	snprintf(fname, sizeof(fname), "%s/%s", hotfile_prefix, p);
	pp.f = fname;
	char fcname[256] = {0x0};
	snprintf(fcname, sizeof(fcname), "%s:%s", pp.d, fname);
	pp.fc = fcname;
	if (strcmp("all", p1) == 0)
		do_syncfile(ALL, &pp, buf, &l);
	else if (strcmp("allhot", p1) == 0)
		do_syncfile(ALLHOT, &pp, buf, &l);
	else if (strcmp("alltelhot", p1) == 0)
		do_syncfile(TEL, &pp, buf, &l);
	else if (strcmp("allcnchot", p1) == 0)
		do_syncfile(CNC, &pp, buf, &l);
	else if (strcmp("yd", p1) == 0)
		do_syncfile(YD, &pp, buf, &l);
	else if (strcmp("hs", p1) == 0)
		do_syncfile(HS, &pp, buf, &l);
	else if (strcmp("edu", p1) == 0)
		do_syncfile(EDU, &pp, buf, &l);
	else if (strcmp("tt", p1) == 0)
		do_syncfile(TT, &pp, buf, &l);
	else if (strcmp("mp4", p1) == 0)
		do_syncfile(MP4, &pp, buf, &l);
	else if (isalpha(*p1))
		do_syncfile(get_isp(p1), &pp, buf, &l);
	else
		do_syncfile_iplist(p1, &pp, buf, &l);
	if (l)
	{
		put_sync_info_2_voss(buf, l);
		l = snprintf(buf, len, "OK!\n");
	}
	else
		l = snprintf(buf, len, "OK!\n");
	return l;
}

static void del_from_shm(t_cs_dir_info *cs, char *domain, char *f, uint32_t *ips, int *idx, int delall)
{
	uint32_t *ip = ips + *idx;
	char t[256] = {0x0};
	snprintf(t, sizeof(t), "%s:%s", domain, f);
	t_cdc_data *d;
	if (find_cdc_node(t, &d))
	{
		LOG(vfs_http_log, LOG_ERROR, "%s shm no record\n", t);
		return ;
	}
	t_cdc_val *v = &(d->v);
	int i = 0;
	for (i = 0; i < MAX_IP_IN_DIR; i++)
	{
		if (v->ip[i] == 0)
			continue;
		int s;
		get_n_s(i, &s, &(v->status_bists));
		if (s != CDC_F_OK)
			continue;
		if (delall == -1)
		{
			if (check_ip_isp(v->ip[i], cs))
				continue;
		}
		if (delall != ALL)
		{
			t_ip_info *ipinfo = NULL;
			if (get_ip_info_by_uint(&ipinfo, v->ip[i], 0, " ", " ") == 0)
			{
				if (ipinfo->isp != delall)
					continue;
			}
		}
		set_n_s(i, CDC_F_DEL, &(v->status_bists));
		v->mtime[i] = time(NULL);
		*ip = v->ip[i];
		LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in shm\n", *ip, domain, f);
		ip++;
		(*idx)++;
	}
}

static void del_from_redis(t_cs_dir_info *cs, char *d, char *f, uint32_t *ips, int *idx, int delall)
{
	LOG(vfs_http_log, LOG_NORMAL, "delete %s %s prepare %d\n", d, f, *idx);
	uint32_t *ip = ips + *idx;
    uint32_t tips[1024] = {0x0};
    get_redis(d, f, tips, sizeof(tips), c);
    int i = 0;
    uint32_t *tip = tips;
    for(; i < 1024 && *tip; tip++, i++){
        if (*tip == 0)
			return;
		if (delall == -1 && check_ip_isp(*tip, cs) != 0)
			continue;
		if (delall != ALL)
		{
			t_ip_info *ipinfo = NULL;
			if (get_ip_info_by_uint(&ipinfo, *tip, 0, " ", " ") == 0)
			{
				if (ipinfo->isp != delall)
					continue;
			}
		}
		if (del_ip_redis(d, f, *tip, c))
			LOG(vfs_http_log, LOG_ERROR, "delete %u %s %s err\n", *tip, d, f);
		else
			LOG(vfs_http_log, LOG_NORMAL, "delete %u %s %s ok in redis\n", *tip, d, f);
		*ip = *tip;
		ip++;
		(*idx)++;
	}
}

int delfile_p(char *url, char *buf, int len)
{
	char *t = url;
	int l = 0; 
	int all_isp_del = -1;
	char *p = NULL;
	if ((p = strstr(t, "&deltype=")) != NULL)
	{
		all_isp_del = get_isp_by_name(p+9);
		if (all_isp_del == UNKNOW_ISP)
		{
			LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
			l += snprintf(buf+l, len -l , "err format delfile\n");
			return l;
		}
		*p = '\0';
	}

	p = strchr(t, '/');
	if ( p == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	*p = 0x0;
	char domain[64] = {0x0};
	snprintf(domain, sizeof(domain), "%s", t);
	if (strncmp(domain, "fcs", 3) || !strstr(domain, ".56.com"))
	{
		LOG(vfs_http_log, LOG_ERROR, "err format delfile %s\n", t);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	p++;
	char fname[256] = {0x0};
	snprintf(fname, sizeof(fname), "%s/%s", hotfile_prefix, p);

	t_cs_dir_info cs;
	memset(&cs, 0, sizeof(cs));
	if (get_cs_info_by_path(fname, &cs))
	{
		LOG(vfs_http_log, LOG_ERROR, "get_cs_info_by_path err %s %m\n", fname);
		l += snprintf(buf+l, len -l , "err format delfile\n");
		return l;
	}
	uint32_t delip[1024] = {0x0};
	int i = 0;
	int idx = 0;
	if (all_isp_del == ALL)
	{
		while (i < cs.index)
		{
			delip[i] = cs.ip[i];
			idx++;
			i++;
		}
	}
	del_from_shm(&cs, domain, fname, delip, &idx, all_isp_del);
	del_from_redis(&cs, domain, fname, delip, &idx, all_isp_del);

	uint32_t *ip = delip;
	i = 0;
	while (i < 1024)
	{
		if (*ip == 0)
			break;
		char sip[16] = {0x0};
		ip2str(sip, *ip);
		LOG(vfs_http_log, LOG_NORMAL, "%s %s %s be delete\n", sip, domain, fname);
		l += snprintf(buf+l, len -l , "iplist=%s&vfs_cmd=M_DELFILE&%s=%s\n", sip, domain, fname);
		i++;
		ip++;
	}

	put_sync_info_2_voss(buf, l);
	l = snprintf(buf, len, "OK!\n");
	return l;
}

static const char *URL_PREFIX[] = {"&fileinfo=", "&ipinfo=", "&taskinfo=", "&hotfile=", "&delfile="};
static const int URL_PRELEN[] = {10, 8, 10, 9, 9};
static const http_request_cb cball[] = {fileinfo_p, ipinfo_p, taskinfo_p, hotfile_p, delfile_p};

int svc_init() 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./http_log.log";

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_http_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_http_log < 0)
		return -1;
	LOG(vfs_http_log, LOG_DEBUG, "svc_init init log ok!\n");
	if (link_cdc_write())
	{
		LOG(vfs_http_log, LOG_ERROR, "link_cdc_write ERR %m!\n");
		return -1;
	}
	init_cdc_rule();
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&iplist[i]);
		INIT_LIST_HEAD(&tasklist[i]);
	}
	INIT_LIST_HEAD(&taskhome);

	t_cdc_http_task *taskall = (t_cdc_http_task*) malloc(sizeof(t_cdc_http_task) * PRE_MALLOC);
	if (taskall == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return -1;
	}
	memset(taskall, 0, sizeof(t_cdc_http_task) * PRE_MALLOC);
	t_cdc_http_task *ttask = taskall;
	for (i = 0; i < PRE_MALLOC; i++)
	{
		INIT_LIST_HEAD(&(ttask->iplist));
		INIT_LIST_HEAD(&(ttask->tlist));
		list_add(&(ttask->tlist), &taskhome);
		ttask++;
	}
	memset(&cdc_path, 0, sizeof(cdc_path));

	char *v = myconfig_get_value("path_indir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_indir!\n");
		return -1;
	}
	snprintf(cdc_path.indir, sizeof(cdc_path.indir), "%s", v);

	v = myconfig_get_value("path_outdir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_outdir!\n");
		return -1;
	}
	snprintf(cdc_path.outdir, sizeof(cdc_path.outdir), "%s", v);

	v = myconfig_get_value("path_voss_tmpdir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_voss_tmpdir!\n");
		return -1;
	}
	snprintf(voss_tmpdir, sizeof(voss_tmpdir), "%s", v);

	v = myconfig_get_value("path_voss_indir");
	if (v == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "config have not path_voss_indir!\n");
		return -1;
	}
	snprintf(voss_indir, sizeof(voss_indir), "%s", v);

	v = myconfig_get_value("path_hotfile_prefix");
	if (v == NULL)
		snprintf(hotfile_prefix, sizeof(hotfile_prefix), "/home/webadm/htdocs");
	else
		snprintf(hotfile_prefix, sizeof(hotfile_prefix), "%s", v);

	sendbuf = (char *) malloc(SENDBUFSIZE);
	if (sendbuf == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc %d err %m\n", SENDBUFSIZE);
		return -1;
	}
	return init_db() || reload_trust_ip() || init_select_ip() || init_check_dup();
}

static int check_trust_ip(uint32_t ip)
{
	char sip[16] = {0x0};
	ip2str(sip, ip);
	if(strncmp(sip, "10.", 3) == 0)
		return 0;

	uint32_t index = ip & IPMODE;
	int i = 0;
	while (i < 1024)
	{
		if (trustip[index][i] == ip)
			return 0;
		if (trustip[index][i] == 0)
			return -1;
		i++;
	}
	return -1;
}

int svc_initconn(int fd) 
{
	LOG(vfs_http_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	if (check_trust_ip(getpeerip(fd)))
	{
		char sip[16] = {0x0};
		ip2str(sip, getpeerip(fd));
		char buf[128] = {0x0};
		snprintf(buf, sizeof(buf), "IP:%s try push file!", sip);
		SetStr(VFS_UNTRUST_IP, buf);
		LOG(vfs_http_log, LOG_ERROR, "%s\n", buf);
		return RET_CLOSE_MALLOC;
	}
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(1024);
	if (curcon->user == NULL)
	{
		LOG(vfs_http_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	LOG(vfs_http_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

static int check_request(int fd, char* data, int len) 
{
	if(len < 14)
		return 0;
		
	struct conn *c = &acon[fd];
	if(!strncmp(data, "GET /", 5)) {
		char* p;
		if((p = strstr(data + 5, "\r\n\r\n")) != NULL) {
			char* q;
			int len;
			if((q = strstr(data + 5, " HTTP/")) != NULL) {
				len = q - data - 5;
				if(len < 1023) {
					strncpy(c->user, data + 5, len);	
					((char*)c->user)[len] = '\0';
					return p - data + 4;
				}
			}
			return -2;	
		}
		else
			return 0;
	}
	else
		return -1;
}

static int get_result(char *url, char *buf, int len)
{
	int index = -1;

	int max = sizeof(URL_PREFIX) / sizeof(char*);
	int i = 0;
	char *t = url;
	for(i = 0; i < max; i++)
	{
		if(strncmp(url, URL_PREFIX[i], URL_PRELEN[i]) == 0)
		{
			index = i;
			t += URL_PRELEN[i];
			break;
		}
	}
	if (index < 0)
		return index;

	return cball[index](t, buf, len);
}

static int handle_request(int cfd) 
{
	char httpheader[1024] = {0x0};
	struct conn *c = &acon[cfd];
	char *url = (char*)c->user;
	LOG(vfs_http_log, LOG_NORMAL, "[%u] url = %s\n", getpeerip(cfd), url);

	int n = get_result(url, sendbuf, SENDBUFSIZE);
	if (n <= 0)
	{
		LOG(vfs_http_log, LOG_ERROR, "err request %s\n", url);
		return RECV_CLOSE;
	}
	snprintf(httpheader, sizeof(httpheader), "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\n\r\n", n);
	set_client_data(cfd, httpheader, strlen(httpheader));
	if (n > 0)
		set_client_data(cfd, sendbuf, n);
	return RECV_SEND;
}

static int check_req(int fd)
{
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen == 0)
	{
		LOG(vfs_http_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	int ret = handle_request(fd);
	consume_client_data(fd, clen);
	return ret;
}

int svc_pinit()
{
	return 0;
}

int svc_recv(int fd) 
{
	return check_req(fd);
}

int svc_send(int fd)
{
	return SEND_CLOSE;
}

void svc_timeout()
{
	time_t cur = time(NULL);
	static time_t last = 0;
	if (cur - last >= 1800)
	{
		last = cur;
		reload_trust_ip();
		refresh_run_task();
		clear_expire();
		clear_dup_expire();
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_http_log, LOG_DEBUG, "close %d\n", fd);
}
