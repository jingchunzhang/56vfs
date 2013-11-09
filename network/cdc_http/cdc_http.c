/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

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

#include "cdc_http_sub2.c"

static const char *URL_PREFIX[] = {"&fileinfo=", "&ipinfo=", "&taskinfo=", "&hotfile=", "&delfile=", "&cfgquery=", "archive?"};
static const int URL_PRELEN[] = {10, 8, 10, 9, 9, 10, 8};
static const http_request_cb cball[] = {fileinfo_p, ipinfo_p, taskinfo_p, hotfile_p, delfile_p, cfgquery_p, archive_p};

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
		list_add_head(&(ttask->tlist), &taskhome);
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
