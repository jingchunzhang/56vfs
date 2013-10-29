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
#include "GeneralHashFunctions.h"
#include "global.h"
#include "vfs_so.h"
#include "vfs_init.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "bitops.h"
#include "acl.h"
#include "cdc_hash.h"
#include "cdc_http.h"
#include "cdc_http_db.h"
int cdc_r_log = -1;

#define MAX_TRUST 16

#define MAX_HOT_IN_ISP 1024

uint32_t trustip[MAX_TRUST];
#include "cdc_http_to.c"

typedef struct {
	char ip[2048];
} t_cdc_r_ip;

typedef struct {
	char ip[MAX_HOT_IN_ISP][16];
	uint32_t uip[MAX_HOT_IN_ISP];
} t_cdc_hot_ip;

typedef struct {
	uint32_t uip[MAX_HOT_IN_ISP];
} t_hash_hot_ip;

t_cdc_hot_ip g_hot_ip;
t_cdc_hot_ip g_little_isp_ip;

static int p_lispfile_r(t_cdc_data *d, uint32_t *uips, char *buf, int len)
{
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	t_cdc_r_ip ips[MAXISP];
	memset(ips, 0, sizeof(ips));
	int j = 0;
	if (d)
	{
		t_cdc_val *v = &(d->v);
		for (j = 0; j < MAX_IP_IN_DIR; j++)
		{
			if (v->ip[j] == 0)
				continue;
			int s;
			get_n_s(j, &s, &(v->status_bists));
			if (s == CDC_F_DEL)
				continue;
			char ip[16] = {0x0};
			ip2str(ip, v->ip[j]);
			if (get_ip_info_by_uint(&ipinfo, v->ip[j], 1, " ", " "))
			{
				LOG(cdc_r_log, LOG_ERROR, "get_ip_info err %s\n", ip);
				continue;
			}
			if (ipinfo->isp < EDU)
				continue;
			t_cdc_r_ip *sips = &(ips[ipinfo->isp]);
			int subl = strlen(sips->ip);
			if (subl)
				snprintf(sips->ip + subl, sizeof(sips->ip) - subl, ",%s", ip);
			else
				snprintf(sips->ip + subl, sizeof(sips->ip) - subl, "%s", ip);
		}
	}
	for (j = 0; j < 1024; j++, uips++)
	{
		if (*uips == 0)
			break;
		char ip[16] = {0x0};
		ip2str(ip, *uips);
		if (get_ip_info_by_uint(&ipinfo, *uips, 1, " ", " "))
		{
			LOG(cdc_r_log, LOG_ERROR, "get_ip_info err %s\n", ip);
			continue;
		}
		if (ipinfo->isp < EDU)
			continue;
		t_cdc_r_ip *sips = &(ips[ipinfo->isp]);
		int subl = strlen(sips->ip);
		if (subl)
			snprintf(sips->ip + subl, sizeof(sips->ip) - subl, ",%s", ip);
		else
			snprintf(sips->ip + subl, sizeof(sips->ip) - subl, "%s", ip);
	}
	int l = 0;
	for (j = 0; j < MAXISP; j++)
	{
		if (strlen(ips[j].ip))
			l += snprintf(buf + l, len - l, "%d:%s;", j, ips[j].ip);
	}
	if (l == 0)
		l += snprintf(buf + l, len - l, "EMPTY");
	return l;
}

static int p_hotfile_r(int type, char *domain, char *buf, int len)
{
	t_cdc_data *d = NULL;
	if (find_cdc_node(domain, &d))
		LOG(cdc_r_log, LOG_ERROR, "find_cdc_node %s no result in shm!\n", domain);

	char *f = strchr(domain, ':');
	*f = 0x0;
	f++;

	uint32_t uips[1024];
	memset(uips, 0, sizeof(uips));
	get_ip_list_db(uips, 1024, domain, f);
	if (type)
		return p_lispfile_r(d, uips, buf, len);

	t_hash_hot_ip haship[16];
	memset(haship, 0, sizeof(haship));

	int i = 0;
	int j = 0;
	t_hash_hot_ip *huip;
	for (i = 0; i < 1024; i++)
	{
		if (uips[i] == 0)
			break;
		huip = &(haship[uips[i]&0xF]);
		for (j = 0; j < MAX_HOT_IN_ISP; j++)
		{
			if (huip->uip[j] == uips[i])
				break;
			if (huip->uip[j] == 0)
			{
				huip->uip[j] = uips[i];
				break;
			}
		}
	}
	if (d)
	{
		t_cdc_val *v = &(d->v);
		for (i = 0; i < MAX_IP_IN_DIR; i++)
		{
			if (v->ip[i] == 0)
				continue;
			int s;
			get_n_s(i, &s, &(v->status_bists));
			if (s == CDC_F_DEL)
				continue;
			huip = &(haship[v->ip[i]&0xF]);
			for (j = 0; j < MAX_HOT_IN_ISP; j++)
			{
				if (huip->uip[j] == v->ip[i])
					break;
				if (huip->uip[j] == 0)
				{
					huip->uip[j] = v->ip[i];
					break;
				}
			}
		}
	}
	/*start find now*/
	int l = 0;
	l += snprintf(buf + l, len - l, "ip=");
	t_cdc_hot_ip * shot_ip = &g_hot_ip;
	for (i = 0; i < MAX_HOT_IN_ISP; i++)
	{
		if (shot_ip->uip[i] == 0)
			break;
		huip = &(haship[shot_ip->uip[i]&0xF]);
		for (j = 0; j < MAX_HOT_IN_ISP; j++)
		{
			if (huip->uip[j] == shot_ip->uip[i])
				break;
			if (len - l < 16)
			{
				LOG(cdc_r_log, LOG_ERROR, "too long %d %d\n", l, len);
				return l;
			}
			if (huip->uip[j] == 0)
			{
				l += snprintf(buf + l, len - l, "%s,", shot_ip->ip[i]);
				break;
			}
		}
	}
	return l;
}

int rfile_p(char *url, char *buf, int len)
{
	char *t = strchr(url, '/');
	int l = 0;
	if (t == NULL)
	{
		LOG(cdc_r_log, LOG_ERROR, "format err %s\n", url);
		l += snprintf(buf + l, len - l, "ERROR");
		return l;
	}
	*t = 0x0;
	char domain[64] = {0x0};
	snprintf(domain, sizeof(domain), "%s", url);
	t++;
	char *t2 = NULL;
	char *t1 = strstr(t, "&hotfile");
	if (t1)
		*t1 = 0x0;
	else if ((t2 = strstr(t, "&lispfile")))
		*t2 = 0x0;

	char fname[256] = {0x0};
	snprintf(fname, sizeof(fname), "%s:/home/webadm/htdocs/%s", domain, t);
	if (t1)
		return p_hotfile_r(0, fname, buf, len);
	else if (t2)
		return p_hotfile_r(1, fname, buf, len);

	t_ip_info ipinfo0;
	t_cdc_data *d = NULL;
	if (find_cdc_node(fname, &d))
	{
		LOG(cdc_r_log, LOG_ERROR, "find_cdc_node %s no result!\n", fname);
		l += snprintf(buf + l, len - l, "EMPTY");
		return l;
	}
	t_ip_info *ipinfo = &ipinfo0;
	t_cdc_val *v = &(d->v);
	t_cdc_r_ip ips[MAXISP];
	memset(ips, 0, sizeof(ips));
	int j = 0;
	for (j = 0; j < MAX_IP_IN_DIR; j++)
	{
		if (v->ip[j] == 0)
			continue;
		int s;
		get_n_s(j, &s, &(v->status_bists));
		if (s == CDC_F_DEL)
			continue;
		char ip[16] = {0x0};
		ip2str(ip, v->ip[j]);
		if (get_ip_info_by_uint(&ipinfo, v->ip[j], 1, " ", " "))
		{
			LOG(cdc_r_log, LOG_ERROR, "get_ip_info err %s\n", ip);
			continue;
		}
		t_cdc_r_ip *sips = &(ips[ipinfo->isp]);
		int subl = strlen(sips->ip);
		if (subl)
			snprintf(sips->ip + subl, sizeof(sips->ip) - subl, ",%s", ip);
		else
			snprintf(sips->ip + subl, sizeof(sips->ip) - subl, "%s", ip);
	}
	for (j = 0; j < MAXISP; j++)
	{
		if (strlen(ips[j].ip))
			l += snprintf(buf + l, len - l, "%d:%s;", j, ips[j].ip);
	}
	if (l == 0)
		l += snprintf(buf + l, len - l, "EMPTY");
	return l;
}

static const char *URL_PREFIX[] = {"&rfile="};
static const int URL_PRELEN[] = {7};
static const http_request_cb cball[] = {rfile_p};

static int init_trust_ip()
{
	memset(trustip, 0, sizeof(trustip));
	int i = 0;
	char *s = myconfig_get_value("cdc_trust_ip");
	if (s == NULL)
	{
		LOG(cdc_r_log, LOG_ERROR, "no cdc_trust_ip!\n");
		return -1;
	}
	char *e = NULL;
	while (1)
	{
		e = strchr(s, ',');
		if (e == NULL)
			break;
		*e = 0x0;
		e++;
		trustip[i] = str2ip(s);
		i++;
		if (i >= MAX_TRUST)
		{
			LOG(cdc_r_log, LOG_ERROR, "too many trust ip %s", myconfig_get_value("cdc_trust_ip"));
			return -1;
		}
		s = e;
	}
	trustip[i] = str2ip(s);
	return 0;
}

static int refresh_hot_ip()
{
	memset(&g_little_isp_ip, 0, sizeof(g_little_isp_ip));
	memset(&g_hot_ip, 0, sizeof(g_hot_ip));
	if (get_cfg_lock())
	{
		LOG(cdc_r_log, LOG_ERROR, "get_hot_ips err , try later!\n");
		return -1; 
	}
	t_ip_info_list *server = NULL;
	list_head_t *l;
	int i = 0;
	list_for_each_entry_safe_l(server, l, &hothome, hotlist)
	{
		g_hot_ip.uip[i] = server->ipinfo.ip;
		snprintf(g_hot_ip.ip[i], sizeof(g_hot_ip.ip[i]), "%s", server->ipinfo.sip);
		i++;
	}
	int j = 0;
	i = 0;
	for (; j < 256; j++)
	{
		list_head_t *hashlist = &(cfg_iplist[j]);
		list_for_each_entry_safe_l(server, l, hashlist, hlist)
		{
			if (server->ipinfo.isp < EDU)
				continue;
			if (server->ipinfo.isp == MP4)
				continue;
			g_little_isp_ip.uip[i] = server->ipinfo.ip;
			snprintf(g_little_isp_ip.ip[i], sizeof(g_little_isp_ip.ip[i]), "%s", server->ipinfo.sip);
			i++;
		}
	}
	release_cfg_lock();
	return 0;
}

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
	cdc_r_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (cdc_r_log < 0)
		return -1;
	LOG(cdc_r_log, LOG_DEBUG, "svc_init init log ok!\n");
	if (link_cdc_write())
	{
		LOG(cdc_r_log, LOG_ERROR, "link_cdc_write ERR %m!\n");
		return -1;
	}
	uint32_t h1,h2,h3;
	get_3_hash("aa", &h1, &h2, &h3);

	return init_trust_ip() || refresh_hot_ip() || init_r_to();
}

int svc_pinit()
{
	return init_db() || init_p_to();
}

static int check_ip(uint32_t ip)
{
	char sip[16] = {0x0};
	ip2str(sip, ip);
	if(strncmp(sip, "10.", 3) == 0)
		return 0;

	int i = 0;
	for (i = 0; i < MAX_TRUST; i++)
	{
		if (trustip[i] == 0)
			return -1;
		if (trustip[i] == ip)
			return 0;
	}
	return -1;
}

int svc_initconn(int fd) 
{
	LOG(cdc_r_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	if (check_ip(getpeerip(fd)))
	{
		LOG(cdc_r_log, LOG_ERROR, "ip %u not in trust!\n", getpeerip(fd));
		return RET_CLOSE_MALLOC;
	}
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(t_r_peer));
	if (curcon->user == NULL)
	{
		LOG(cdc_r_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	t_r_peer *peer = (t_r_peer *)curcon->user;
	memset(peer, 0, sizeof(t_r_peer));
	peer->fd = fd;
	add_2_active(peer);
	LOG(cdc_r_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
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
	{
		LOG(cdc_r_log, LOG_ERROR, "can get cb %s\n", url);
		return index;
	}

	return cball[index](t, buf, len);
}

static int handle_request(int cfd) 
{
	char httpheader[256] = {0x0};
	struct conn *c = &acon[cfd];
	char *url = (char*)c->user;
	LOG(cdc_r_log, LOG_NORMAL, "url = %s\n", url);

	char sendbuf[4096] = {0x0};
	int n = get_result(url, sendbuf, sizeof(sendbuf));
	if (n <= 0)
	{
		LOG(cdc_r_log, LOG_ERROR, "err request %s\n", url);
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
		LOG(cdc_r_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	int clen = check_request(fd, data, datalen);
	if (clen < 0)
	{
		LOG(cdc_r_log, LOG_DEBUG, "fd[%d] data error ,not http!\n", fd);
		return RECV_CLOSE;
	}
	if (clen == 0)
	{
		LOG(cdc_r_log, LOG_DEBUG, "fd[%d] data not suffic!\n", fd);
		return RECV_ADD_EPOLLIN;
	}
	int ret = handle_request(fd);
	consume_client_data(fd, clen);
	return ret;
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
	scan_to();
}

void svc_finiconn(int fd)
{
	LOG(cdc_r_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	t_r_peer *peer = (t_r_peer *)curcon->user;
	if (peer == NULL)
		return;
	list_del_init(&(peer->alist));
}
