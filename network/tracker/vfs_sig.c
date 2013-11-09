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
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "vfs_sig.h"
#include "util.h"
#include "acl.h"

int vfs_sig_log = -1;
int vfs_sig_log_err = -1;
extern uint8_t self_stat ;
extern t_ip_info self_ipinfo;

/* online list */
static list_head_t activelist;  //用来检测超时
static list_head_t online_list[256]; //用来快速定位查找
static list_head_t cfg_list[256]; //配置IP定位

int svc_initconn(int fd); 
int active_send(vfs_tracker_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b);

static int init_cfg_connect(char *sip, vfs_tracker_peer *peer)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	if (ip == INADDR_NONE)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "err ip %s %u\n", sip, peer->ip);
		return -1;
	}
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	if (get_ip_info(&ipinfo, s_ip, 1))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "get_ip_info err ip %s %s %u\n", sip, s_ip, peer->ip);
		return -1;
	}
	peer->cfgip = ip;
	peer->sock_stat = LOGIN;
	peer->role = ipinfo->role;
	peer->isp = ipinfo->isp;
	peer->archive_isp = ipinfo->archive_isp;
	peer->real_isp = ipinfo->real_isp;
	list_add_head(&(peer->cfglist), &cfg_list[ip&ALLMASK]);
	return 0;
}

#include "vfs_sig_base.c"
#include "vfs_sig_sub.c"

int svc_init() 
{
	char *logname = myconfig_get_value("log_sig_logname");
	if (!logname)
		logname = "./sig_log.log";

	char *cloglevel = myconfig_get_value("log_sig_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_sig_logsize", 100);
	int logintval = myconfig_get_intval("log_sig_logtime", 3600);
	int lognum = myconfig_get_intval("log_sig_lognum", 10);
	vfs_sig_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_sig_log < 0)
		return -1;
	char errlogfile[256] = {0x0};
	snprintf(errlogfile, sizeof(errlogfile), "%s.err", logname);
	vfs_sig_log_err = registerlog(errlogfile, loglevel, logsize, logintval, lognum);
	if (vfs_sig_log_err < 0)
		return -1;
	LOG(vfs_sig_log, LOG_DEBUG, "svc_init init log ok!\n");
	INIT_LIST_HEAD(&activelist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
		INIT_LIST_HEAD(&cfg_list[i]);
	}
	
	self_stat = ON_LINE;
	return 0;
}

int svc_initconn(int fd) 
{
	LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	uint32_t ip = getpeerip(fd);
	vfs_tracker_peer *peer = NULL;
	if (find_ip_stat(ip, &peer) == 0)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "fd %d ip %u dup connect!\n", fd, ip);
		return RET_CLOSE_DUP;
	}
	LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_tracker_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(vfs_tracker_peer));
	peer = (vfs_tracker_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	peer->ip = ip;
	INIT_LIST_HEAD(&(peer->alist));
	INIT_LIST_HEAD(&(peer->hlist));
	INIT_LIST_HEAD(&(peer->cfglist));
	INIT_LIST_HEAD(&(peer->tasklist));
	list_move_tail(&(peer->alist), &activelist);
	list_add_head(&(peer->hlist), &online_list[ip&ALLMASK]);
	LOG(vfs_sig_log, LOG_TRACE, "a new fd[%d] init ok!\n", fd);
	return 0;
}

/*校验是否有一个完整请求*/
static int check_req(int fd)
{
	t_vfs_sig_head h;
	t_vfs_sig_body b;
	memset(&b, 0, sizeof(b));
	memset(&h, 0, sizeof(h));
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_sig_log, LOG_TRACE, "fd[%d] no data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	int ret = parse_sig_msg(&h, &b, data, datalen);
	if (ret > 0)
	{
		LOG(vfs_sig_log, LOG_TRACE, "fd[%d] no suffic data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	if (ret == E_PACKET_ERR_CLOSE)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] ERROR PACKET bodylen is [%d], must be close now!\n", fd, h.bodylen);
		return RECV_CLOSE;
	}
	int clen = h.bodylen + SIG_HEADSIZE;
	ret = do_req(fd, &h, &b);
	consume_client_data(fd, clen);
	return ret;
}

int svc_recv(int fd) 
{
	struct conn *curcon = &acon[fd];
	vfs_tracker_peer *peer = (vfs_tracker_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	
	int ret = RECV_ADD_EPOLLIN;;
	int subret = 0;
	while (1)
	{
		subret = check_req(fd);
		if (subret == -1)
			break;
		if (subret == RECV_CLOSE)
			return RECV_CLOSE;
		ret |= subret;
	}
	return ret;
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_tracker_peer *peer = (vfs_tracker_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	int to = g_config.timeout * 10;
	vfs_tracker_peer *peer = NULL;
	list_head_t *l;
	t_vfs_sig_head h;
	t_vfs_sig_body b;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (now - peer->hbtime < g_config.timeout)
			break;
		if (peer->role != ROLE_CS )
		{
			if (now - peer->hbtime > g_config.timeout && now - peer->hbtime < to)
			{
				h.bodylen = sizeof(self_stat);
				memcpy(b.body, &self_stat, sizeof(self_stat));
				h.cmdid = HEARTBEAT_REQ;
				if (peer->role == ROLE_TRACKER)
					h.status = HB_T_2_T;
				else
					h.status = HB_T_2_F;
				active_send(peer, &h, &b);
				continue;
			}
		}
		if (now - peer->hbtime > to)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "timeout close %d [%lu:%lu] %d\n", peer->fd, now, peer->hbtime, peer->role);
			do_close(peer->fd);
		}
	}
	scan_cfg_iplist_and_connect();
	check_task();

	static time_t last_scan_timeout = 0;
	if ( now - last_scan_timeout > 1800)
	{
		last_scan_timeout = now;
		do_timeout_task();
	}
}

static void refresh_cs_task(list_head_t *mlist)
{
	t_vfs_tasklist *tasklist = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(tasklist, l, mlist, userlist)
	{
		list_del_init(&(tasklist->userlist));
		vfs_set_task(tasklist, TASK_WAIT);
		LOG(vfs_sig_log, LOG_DEBUG, "re dispatch %s %s\n", tasklist->task.base.src_domain, tasklist->task.base.filename);
	}
}

void svc_finiconn(int fd)
{
	LOG(vfs_sig_log, LOG_TRACE, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_tracker_peer *peer = (vfs_tracker_peer *) curcon->user;
	list_del_init(&(peer->alist));
	list_del_init(&(peer->hlist));
	list_del_init(&(peer->cfglist));
	if (peer->role == ROLE_CS)
		refresh_cs_task(&(peer->tasklist));
	list_del_init(&(peer->tasklist));
	memset(curcon->user, 0, sizeof(vfs_tracker_peer));
}
