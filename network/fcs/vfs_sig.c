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
#include <sys/stat.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include <pthread.h>
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "vfs_sig.h"
#include "vfs_task.h"
#include "vfs_del_file.h"
#include "util.h"
#include "acl.h"

int vfs_sig_log = -1;
extern uint8_t self_stat ;
time_t fcs_start_time;
extern t_g_config g_config;
extern char *iprole[]; 

/* FLV目录监控 */
#define MAX_WATCH 10240
char watch_dirs[MAX_WATCH][256];

int inotify_fd = -1;
fd_set fds;
uint32_t mask = IN_CLOSE_WRITE|IN_DELETE|IN_MOVED_FROM|IN_MOVED_TO;
extern t_ip_info self_ipinfo;
t_sync_para sync_para;
extern char hostname[64];

/* online list */
static list_head_t activelist;  //用来检测超时
static list_head_t trackerlist;  //用来检测超时
static list_head_t online_list[256]; //用来快速定位查找
static list_head_t cfg_list[256]; //配置IP定位
static list_head_t sync_list;  //初始化的待同步任务，不解析具体的源ip
static list_head_t sync_dir_list[256];  //用来检测信令回路 

static pthread_mutex_t sync_dir_mutex = PTHREAD_MUTEX_INITIALIZER;

int svc_initconn(int fd); 
int active_send(vfs_fcs_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b);
#define MAXSYNCBUF 2097152

static int init_cfg_connect(char *sip, vfs_fcs_peer *peer)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	if (ip == INADDR_NONE)
	{
		LOG(vfs_sig_log, LOG_ERROR, "err ip %s %u\n", sip, peer->ip);
		return -1;
	}
	t_ip_info ipinfo0;
	memset(&ipinfo0, 0, sizeof(t_ip_info));
	t_ip_info *ipinfo = &ipinfo0;
	if (get_ip_info(&ipinfo, s_ip, 1))
		LOG(vfs_sig_log, LOG_ERROR, "get_ip_info err ip %s %s %u\n", sip, s_ip, peer->ip);
	peer->cfgip = ip;
	peer->sock_stat = LOGIN;
	if (ipinfo)
	{
		peer->role = ipinfo->role;
		LOG(vfs_sig_log, LOG_NORMAL, "ip:%s:%s role is %s\n", sip, s_ip, iprole[ipinfo->role]); 
	}
	if (peer->role == ROLE_TRACKER)
	{
		list_del_init(&(peer->tlist));
		list_add_head(&(peer->tlist), &trackerlist);
	}
	list_add_head(&(peer->cfglist), &cfg_list[ip&ALLMASK]);
	return 0;
}

#include "vfs_sig_base.c"
#include "vfs_sig_sub.c"
#include "vfs_sig_inotify.c"

int svc_init()  
{
	fcs_start_time = time(NULL);
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
	LOG(vfs_sig_log, LOG_DEBUG, "svc_init init log ok!\n");

	INIT_LIST_HEAD(&activelist);
	INIT_LIST_HEAD(&trackerlist);
	int i = 0;
	for (i = 0; i < 256; i++)
	{
		INIT_LIST_HEAD(&online_list[i]);
		INIT_LIST_HEAD(&cfg_list[i]);
		INIT_LIST_HEAD(&sync_dir_list[i]);
	}
	
	self_stat = ON_LINE;

	INIT_LIST_HEAD(&sync_list);

	//起动FLV目录文件监控线程
	pthread_t tid;	
	int rc;	

	LOG(vfs_sig_log, LOG_DEBUG, "inotify thread start ....\n");
	if((rc = pthread_create(&tid, NULL, (void*(*)(void*))start_inotify_thread, NULL)) != 0)	
	{
		LOG(vfs_sig_log, LOG_ERROR, "inotify thread start error:%d\n", rc);
		return -1;	
	}
	LOG(vfs_sig_log, LOG_DEBUG, "inotify thread start finish, result:%d\n", rc);

	LOG(vfs_sig_log, LOG_DEBUG, "syncdir thread start ....\n");
	if((rc = pthread_create(&tid, NULL, (void*(*)(void*))sync_dir_thread, NULL)) != 0)	
	{
		LOG(vfs_sig_log, LOG_ERROR, "syncdir thread start error:%d\n", rc);
		return -1;	
	}
	LOG(vfs_sig_log, LOG_DEBUG, "syncdir thread start finish, result:%d\n", rc);
	memset(&sync_para, 0, sizeof(sync_para));
	return 0;
}

int svc_initconn(int fd) 
{
	uint32_t ip = getpeerip(fd);
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	vfs_fcs_peer *peer = NULL;
	if (find_ip_stat(ip, &peer) == 0)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd %d ip %u dup connect!\n", fd, ip);
		return RET_CLOSE_DUP;
	}
	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		curcon->user = malloc(sizeof(vfs_fcs_peer));
	if (curcon->user == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "malloc err %m\n");
		return RET_CLOSE_MALLOC;
	}
	memset(curcon->user, 0, sizeof(vfs_fcs_peer));
	peer = (vfs_fcs_peer *)curcon->user;
	peer->hbtime = time(NULL);
	peer->sock_stat = CONNECTED;
	peer->fd = fd;
	peer->ip = ip;
	INIT_LIST_HEAD(&(peer->alist));
	INIT_LIST_HEAD(&(peer->hlist));
	INIT_LIST_HEAD(&(peer->tlist));
	INIT_LIST_HEAD(&(peer->cfglist));
	INIT_LIST_HEAD(&(peer->tasklist));
	list_move_tail(&(peer->alist), &activelist);
	list_add_head(&(peer->hlist), &online_list[ip&ALLMASK]);
	LOG(vfs_sig_log, LOG_DEBUG, "a new fd[%d] init ok!\n", fd);
	return 0;
}

/*校验是否有一个完整请求*/
static int check_req(int fd)
{
	t_vfs_sig_head h;
	t_vfs_sig_body b;
	memset(&h, 0, sizeof(h));
	memset(&b, 0, sizeof(b));
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	int ret = parse_sig_msg(&h, &b, data, datalen);
	if(ret > 0)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] no suffic data!\n", fd);
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
	struct conn *curcon = &acon[fd];
	vfs_fcs_peer *peer = (vfs_fcs_peer *) curcon->user;
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
	vfs_fcs_peer *peer = (vfs_fcs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	return SEND_ADD_EPOLLIN;
}

void svc_timeout()
{
	time_t now = time(NULL);
	int to = g_config.timeout * 10;
	vfs_fcs_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (now - peer->hbtime < g_config.timeout)
			break;
		if (now - peer->hbtime > to)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "timeout close %d [%lu:%lu] %d\n", peer->fd, now, peer->hbtime, peer->role);
			do_close(peer->fd);
		}
	}
	check_task();
	do_fin_task();
	init_sync_list();
	do_active_sync();
	scan_sync_dir_list();

	static time_t last_scan_timeout = 0;
	if ( now - last_scan_timeout > 1800)
	{
		last_scan_timeout = now;
		do_timeout_task();
	}
}

static void refresh_tracker_task(list_head_t *mlist)
{
	t_vfs_tasklist *tasklist = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(tasklist, l, mlist, userlist)
	{
		list_del_init(&(tasklist->userlist));
		LOG(vfs_sig_log, LOG_DEBUG, "refresh_tracker_task %s\n", tasklist->task.base.filename); 
		vfs_set_task(tasklist, TASK_WAIT_SYNC);
	}	
}

void svc_finiconn(int fd)
{
	LOG(vfs_sig_log, LOG_DEBUG, "close %d\n", fd);
	struct conn *curcon = &acon[fd];
	if (curcon->user == NULL)
		return;
	vfs_fcs_peer *peer = (vfs_fcs_peer *) curcon->user;
	list_del_init(&(peer->alist));
	list_del_init(&(peer->hlist));
	list_del_init(&(peer->tlist));
	list_del_init(&(peer->cfglist));
	if (peer->role == ROLE_TRACKER)
		refresh_tracker_task(&(peer->tasklist));
	list_del_init(&(peer->tasklist));
	memset(curcon->user, 0, sizeof(vfs_fcs_peer));
	free(curcon->user);
	curcon->user = NULL;
}
