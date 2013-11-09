/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <string.h>
#include <stdlib.h>
#include <libgen.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "vfs_voss.h"
#include "util.h"
#include "acl.h"
#include "vfs_task.h"
#include "vfs_localfile.h"

typedef struct {
	int fd;
	char file[256];
	time_t last;
} t_client_stat;
t_client_stat c_stat;

extern const char *s_server_stat[STAT_MAX];
int vfs_voss_log = -1;

int voss_stat_interval = 120;

/* online list */
static list_head_t activelist;  //用来检测超时
static list_head_t online_list[256]; //用来快速定位查找
static list_head_t cfg_list[256]; //配置定位查找

#include "vfs_voss_sub.c"

const char *sock_stat_cmd[] = {"LOGOUT", "CONNECTED", "LOGIN", "HB_SEND", "HB_RSP", "IDLE", "RECV_LAST", "SEND_LAST", "PREPARE_SYNCFILE", "SYNCFILEING", "SYNCFILE_POST", "SYNCFILE_OK", "PREPARE_SENDFILE", "SENDFILEING", "SENDFILE_OK"};

static int check_stat_file()
{
	time_t now = time(NULL);
	if (c_stat.fd >= 0)
	{
		if (now - c_stat.last >= voss_stat_interval)
		{
			close (c_stat.fd);
			c_stat.fd = -1;

			char outfile[256] = {0x0};
			char workfile[256] = {0x0};
			snprintf(workfile, sizeof(workfile), "%s", c_stat.file);
			snprintf(outfile, sizeof(outfile), "%s/%s/%s", g_config.path, path_dirs[PATH_OUTDIR], basename(c_stat.file));
			if (rename(workfile, outfile))
				LOG(vfs_voss_log, LOG_ERROR, "rename [%s][%s] err %m\n", workfile, outfile);
		}
		else
			return 0;
	}

	if (c_stat.fd < 0)
	{
		char day[16] = {0x0};
		get_strtime(day);
		memset(c_stat.file, 0, sizeof(c_stat.file));
		snprintf(c_stat.file, sizeof(c_stat.file), "%s/%s/%sstat_%s", g_config.path, path_dirs[PATH_WKDIR], VOSSPREFIX, day);
		c_stat.fd = open(c_stat.file, O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE, 0644);
		if (c_stat.fd < 0)
		{
			LOG(vfs_voss_log, LOG_ERROR, "open %s err %m\n", c_stat.file);
			return -1;
		}
		c_stat.last = time(NULL);
	}
	return 0;
}

int svc_init() 
{
	char *logname = myconfig_get_value("log_voss_logname");
	if (!logname)
		logname = "./voss_log.log";

	char *cloglevel = myconfig_get_value("log_voss_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_voss_logsize", 100);
	int logintval = myconfig_get_intval("log_voss_logtime", 3600);
	int lognum = myconfig_get_intval("log_voss_lognum", 10);
	vfs_voss_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_voss_log < 0)
		return -1;
	LOG(vfs_voss_log, LOG_NORMAL, "svc_init init log ok!\n");
	INIT_LIST_HEAD(&activelist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
		INIT_LIST_HEAD(&cfg_list[i]);
	}

	memset(&c_stat, 0, sizeof(c_stat));
	c_stat.fd = -1;
	return check_stat_file();
}

int svc_initconn(int fd) 
{
	LOG(vfs_voss_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	uint32_t ip = getpeerip(fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_voss_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_voss_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	vfs_voss_peer *peer;
	memset(curcon->user, 0, sizeof(vfs_voss_peer));
	peer = (vfs_voss_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	peer->con_ip = ip;
	peer->local_in_fd = -1;
	ip2str(peer->ip, ip);
	INIT_LIST_HEAD(&(peer->alist));
	INIT_LIST_HEAD(&(peer->hlist));
	INIT_LIST_HEAD(&(peer->cfglist));
	list_move_tail(&(peer->alist), &activelist);
	list_add_head(&(peer->hlist), &online_list[ip&ALLMASK]);
	LOG(vfs_voss_log, LOG_TRACE, "a new fd[%d] init ok!\n", fd);
	return 0;
}
/*校验是否有一个完整请求*/
static int check_req(int fd)
{
	LOG(vfs_voss_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	t_head_info h;
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_voss_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
		return -1;  /*no data*/
	}
	int ret = parse_msg(data, datalen, &h);
	if (ret > 0)
	{
		LOG(vfs_voss_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
		return -1;  /*no suffic data, need to get data more */
	}
	if (h.cmdid == REQ_HEARTBEAT || RSP_VFS_CMD == h.cmdid || REQ_AUTH == h.cmdid)
	{
		if (datalen < h.totallen)
		{
			LOG(vfs_voss_log, LOG_DEBUG, "fd[%d] no suffic data!\n", fd);
			return -1;
		}
		if (h.cmdid == REQ_AUTH)
		{
			char sip[128] = {0x0};
			strncpy(sip, data + HEADSIZE, h.totallen - HEADSIZE);
			if (init_cfg_connect(sip, peer))
				return RECV_CLOSE;
		}
		else if (c_stat.fd >= 0)
		{
			char day[16] = {0x0};
			get_strtime(day);
			write(c_stat.fd, day, strlen(day));
			write(c_stat.fd, "|", 1);
			write(c_stat.fd, peer->ip, strlen(peer->ip));
			write(c_stat.fd, "|", 1);
			if (h.cmdid == REQ_HEARTBEAT)
			{
				peer->server_stat = *(data+ HEADSIZE);
				const char *t = s_server_stat[peer->server_stat%STAT_MAX];
				write(c_stat.fd, t, strlen(t));
			}
			else
				write(c_stat.fd, data+HEADSIZE, h.totallen - HEADSIZE);
			write(c_stat.fd, "\n", 1);
		}
		consume_client_data(fd, h.totallen);
		return 0;
	}
	else if (h.cmdid == REQ_SUBMIT)
	{
		return process_data_req(fd, &h);
	}

	LOG(vfs_voss_log, LOG_ERROR, "fd[%d] recv a error cmdid[%x]!\n", fd, h.cmdid);
	return RECV_CLOSE;
}

int svc_recv(int fd) 
{
	int ret = RECV_ADD_EPOLLIN;;
	struct conn *curcon = &acon[fd];
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_voss_log, LOG_TRACE, "fd[%d] sock stat %s!\n", fd, sock_stat_cmd[peer->sock_stat]);
	if (peer->sock_stat == PREPARE_SYNCFILE || peer->sock_stat == SYNCFILEING)
	{
		ret = sub_recv(fd);
		if (ret == RECV_ADD_EPOLLIN || ret == RECV_CLOSE)
			return ret;
	}

	ret = RECV_ADD_EPOLLIN;;
	int subret = 0;
	while (1)
	{
		subret = check_req(fd);
		if (subret == -1)
			break;
		if (subret == RECV_CLOSE)
			return RECV_CLOSE;
		if (subret == RECV_ADD_EPOLLIN)
			return RECV_ADD_EPOLLIN;
	}
	return ret;
}

int svc_send(int fd)
{
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	int to = g_config.timeout * 10;
	vfs_voss_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (peer->fd > 0)
			if (peer->sock_stat != PREPARE_SYNCFILE && peer->sock_stat != SYNCFILEING)
				check_close_local(peer->fd);
		if (now - peer->hbtime > to)
		{
			LOG(vfs_voss_log, LOG_DEBUG, "timeout close %d [%lu:%lu]\n", peer->fd, now, peer->hbtime);
			do_close(peer->fd);
		}
	}
	check_stat_file();
	check_indir();
}

void svc_finiconn(int fd)
{
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	LOG(vfs_voss_log, LOG_NORMAL, "close %s\n", peer->ip);
	t_voss_data_info *datainfo = &(peer->datainfo);
	datainfo->opentime = 0;
	check_close_local(fd);
	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	peer->local_in_fd = -1;
	list_del_init(&(peer->alist));
	list_del_init(&(peer->hlist));
	list_del_init(&(peer->cfglist));
	memset(curcon->user, 0, sizeof(vfs_voss_peer));
}
