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
#include "vfs_data.h"
#include "util.h"
#include "acl.h"
#include "vfs_task.h"
#include "vfs_data_task.h"
#include "vfs_localfile.h"

int vfs_sig_log = -1;
extern uint8_t self_stat ;
extern t_ip_info self_ipinfo;
/* online list */
static list_head_t activelist;  //用来检测超时
static list_head_t online_list[256]; //用来快速定位查找

int g_proxyed = 0;
t_vfs_up_proxy g_proxy;
int svc_initconn(int fd); 
int active_send(vfs_cs_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b);
const char *sock_stat_cmd[] = {"LOGOUT", "CONNECTED", "LOGIN", "IDLE", "PREPARE_RECVFILE", "RECVFILEING", "SENDFILEING", "LAST_STAT"};

#include "vfs_data_base.c"
#include "vfs_data_sub.c"

static int init_proxy_info()
{
	memset(&g_proxy, 0, sizeof(g_proxy));
	if (self_ipinfo.role != ROLE_CS)
		return 0;
	char *v = myconfig_get_value("proxy_isp");
	if (v == NULL)
	{
		LOG(vfs_sig_log, LOG_NORMAL, "no proxy_isp!\n");
		return 0;
	}
	if (strcasestr(v, ispname[self_ipinfo.isp]) == NULL)
	{
		LOG(vfs_sig_log, LOG_NORMAL, "self %s not in proxy_isp %s!\n", ispname[self_ipinfo.isp], v);
		return 0;
	}
	char key[128] = {0x0};
	snprintf(key, sizeof(key), "proxy_%s_host", ispname[self_ipinfo.isp]);
	v = myconfig_get_value(key);
	if (!v)
	{
		LOG(vfs_sig_log, LOG_ERROR, "no %s!\n", key);
		return -1;
	}
	snprintf(g_proxy.host, sizeof(g_proxy.host), "%s", v);

	memset(key, 0, sizeof(key));
	snprintf(key, sizeof(key), "proxy_%s_port", ispname[self_ipinfo.isp]);
	g_proxy.port = myconfig_get_intval(key, 0);
	if (g_proxy.port == 0) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "no %s!\n", key);
		return -1;
	}
	g_proxyed = 1;

	memset(key, 0, sizeof(key));
	snprintf(key, sizeof(key), "proxy_%s_username", ispname[self_ipinfo.isp]);
	v = myconfig_get_value(key);
	if (!v)
		return 0;
	snprintf(g_proxy.username, sizeof(g_proxy.username), "%s", v);

	memset(key, 0, sizeof(key));
	snprintf(key, sizeof(key), "proxy_%s_password", ispname[self_ipinfo.isp]);
	v = myconfig_get_value(key);
	if (!v)
		return 0;
	snprintf(g_proxy.password, sizeof(g_proxy.password), "%s", v);
	return 0;
}

int svc_init() 
{
	char *logname = myconfig_get_value("log_data_logname");
	if (!logname)
		logname = "./data_log.log";

	char *cloglevel = myconfig_get_value("log_data_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_data_logsize", 100);
	int logintval = myconfig_get_intval("log_data_logtime", 3600);
	int lognum = myconfig_get_intval("log_data_lognum", 10);
	vfs_sig_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_sig_log < 0)
		return -1;
	LOG(vfs_sig_log, LOG_NORMAL, "svc_init init log ok!\n");
	INIT_LIST_HEAD(&activelist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
	}
	if (init_proxy_info())
	{
		LOG(vfs_sig_log, LOG_ERROR, "init_proxy_info err!\n");
		return -1;
	}
	if (g_proxyed)
		LOG(vfs_sig_log, LOG_NORMAL, "proxy mode!\n");
	else
		LOG(vfs_sig_log, LOG_NORMAL, "not proxy mode!\n");
	
	return 0;
}

int svc_initconn(int fd) 
{
	uint32_t ip = getpeerip(fd);
	LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_cs_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "malloc err %m\n");
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "malloc err %m");
		SetStr(VFS_MALLOC, val);
		return RET_CLOSE_MALLOC;
	}
	vfs_cs_peer *peer;
	memset(curcon->user, 0, sizeof(vfs_cs_peer));
	peer = (vfs_cs_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	peer->ip = ip;
	peer->mode = CON_PASSIVE;
	INIT_LIST_HEAD(&(peer->alist));
	INIT_LIST_HEAD(&(peer->hlist));
	list_move_tail(&(peer->alist), &activelist);
	list_add_head(&(peer->hlist), &online_list[ip&ALLMASK]);
	LOG(vfs_sig_log, LOG_TRACE, "a new fd[%d] init ok!\n", fd);
	return 0;
}

/*校验是否有一个完整请求*/
static int check_req(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	t_vfs_sig_head h;
	t_vfs_sig_body b;
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_sig_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
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
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] ERROR PACKET bodylen is [%d], must be close now!\n", fd, h.bodylen);
		return RECV_CLOSE;
	}
	int clen = h.bodylen + SIG_HEADSIZE;
	ret = do_req(fd, &h, &b);
	consume_client_data(fd, clen);
	return ret;
}

int svc_recv(int fd) 
{
	char val[256] = {0x0};
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
recvfileing:
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_sig_log, LOG_TRACE, "fd[%d] sock stat %s!\n", fd, sock_stat_cmd[peer->sock_stat]);
	if (peer->sock_stat == RECVFILEING)
	{
		LOG(vfs_sig_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
		char *data;
		size_t datalen;
		t_vfs_tasklist *task0 = peer->recvtask;
		if(task0 == NULL)
		{
			LOG(vfs_sig_log, LOG_ERROR, "recv task is null!\n");
			return RECV_CLOSE;  /* ERROR , close it */
		}
		t_vfs_taskinfo *task = &(peer->recvtask->task);
		if (get_client_data(fd, &data, &datalen))
		{
			LOG(vfs_sig_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
			return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
		}
		LOG(vfs_sig_log, LOG_TRACE, "fd[%d] get file %s:%d!\n", fd, task->base.filename, datalen);
		size_t remainlen = task->sub.processlen - task->sub.lastlen;
		datalen = datalen <= remainlen ? datalen : remainlen ; 
		size_t n = write(peer->local_in_fd, data, datalen);
		if (n < 0)
		{
			LOG(vfs_sig_log, LOG_ERROR, "fd[%d] write error %m close it!\n", fd);
			snprintf(val, sizeof(val), "write err %m");
			SetStr(VFS_WRITE_FILE, val);
			return RECV_CLOSE;  /* ERROR , close it */
		}
		consume_client_data(fd, n);
		task->sub.lastlen += n;
		if (task->sub.lastlen >= task->sub.processlen)
		{
			task->sub.endtime = time(NULL);
			if (close_tmp_check_mv(&(task->base), peer->local_in_fd) != LOCALFILE_OK)
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] get file %s error!\n", fd, task->base.filename);
				task0->task.base.overstatus = OVER_E_MD5;
				peer->recvtask = NULL;
				vfs_set_task(task0, TASK_FIN);
				snprintf(val, sizeof(val), "md5 error %m");
				SetStr(VFS_STR_MD5_E, val);
				return RECV_CLOSE;
			}
			else
			{
				LOG(vfs_sig_log, LOG_NORMAL, "fd[%d:%u] get file %s ok!\n", fd, peer->ip, task->base.filename);
				task0->task.base.overstatus = OVER_OK;
				vfs_set_task(task0, TASK_FIN);
			}
			peer->local_in_fd = -1;
			peer->recvtask = NULL;
			peer->sock_stat = IDLE;
			if (g_proxyed)
			{
				LOG(vfs_sig_log, LOG_NORMAL, "work in proxy! short connect!\n");
				return RECV_CLOSE;
			}
			else
				return RECV_ADD_EPOLLIN;
		}
		else
			return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	
	int ret = RECV_ADD_EPOLLIN;;
	int subret = 0;
	while (1)
	{
		subret = check_req(fd);
		if (subret == -1)
			break;
		if (subret == RECV_ADD_EPOLLOUT)
			return RECV_ADD_EPOLLOUT;
		if (subret == RECV_CLOSE)
			return RECV_CLOSE;

		if (peer->sock_stat == RECVFILEING)
			goto recvfileing;
		if (peer->sock_stat == SENDFILEING)
			return RECV_ADD_EPOLLOUT;
	}
	return ret;
}

int svc_send_once(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	return SEND_ADD_EPOLLIN;
}

int svc_send(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	if (peer->sock_stat == SENDFILEING)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
		t_vfs_tasklist *tasklist = peer->sendtask;
		t_vfs_taskinfo *task = &(peer->sendtask->task);
		if (curcon->send_len >= peer->headlen + task->sub.processlen)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d:%u] send file %s ok!\n", fd, peer->ip, task->base.filename);
			peer->sock_stat = IDLE;
			task->base.overstatus = OVER_OK;
			vfs_set_task(tasklist, TASK_FIN);
			peer->sendtask = NULL;
		}
		else
		{
			LOG(vfs_sig_log, LOG_ERROR, "fd[%d:%u] send file %s error [%d:%d:%d]!\n", fd, peer->ip, task->base.filename, curcon->send_len, peer->headlen, task->sub.processlen);
			task->base.overstatus = OVER_SEND_LEN;
			peer->sock_stat = IDLE;
			vfs_set_task(tasklist, TASK_FIN);
			peer->sendtask = NULL;
			return SEND_CLOSE;
		}
	}
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	if (self_ipinfo.role == ROLE_CS)
	{
		time_t now = time(NULL);
		int to = g_config.timeout * 10;
		vfs_cs_peer *peer = NULL;
		list_head_t *l;
		list_for_each_entry_safe_l(peer, l, &activelist, alist)
		{
			if (peer == NULL)
				continue;   /*bugs */
			if (now - peer->hbtime < to)
				break;
			LOG(vfs_sig_log, LOG_DEBUG, "timeout close %d [%lu:%lu] %d\n", peer->fd, now, peer->hbtime, peer->role);
			do_close(peer->fd);
		}
	}
	check_task();
}

void svc_finiconn(int fd)
{
	LOG(vfs_sig_log, LOG_TRACE, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	peer->local_in_fd = -1;
	list_del_init(&(peer->alist));
	list_del_init(&(peer->hlist));
	t_vfs_tasklist *tasklist = NULL;
	if (peer->sendtask)
	{
		tasklist = peer->sendtask;
		LOG(vfs_sig_log, LOG_ERROR, "abort send %s!\n", tasklist->task.base.filename);
		IncInt(VFS_ABORT_INC, 1);
		tasklist->task.base.overstatus = OVER_SEND_LEN;
		vfs_set_task(tasklist, TASK_FIN);
	}
	if (peer->recvtask)
	{
		tasklist = peer->recvtask;
		LOG(vfs_sig_log, LOG_ERROR, "re execute %s!\n", tasklist->task.base.filename);
		IncInt(VFS_RE_EXECUTE_INC, 1);
		if (g_config.continue_flag)
			tasklist->task.base.offsize += tasklist->task.sub.lastlen;
		else
			real_rm_file(tasklist->task.base.tmpfile);
		tasklist->task.base.retry++;
		vfs_set_task(tasklist, TASK_WAIT);
	}
	memset(curcon->user, 0, sizeof(vfs_cs_peer));
	free(curcon->user);
	curcon->user = NULL;
}
