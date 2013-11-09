/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "common.h"
#include "thread.h"
#include "vfs_agent.h"
#include "vfs_task.h"
#include "myconfig.h"
#include "mybuff.h"
#include "log.h"
#include "pro_voss.h"
#include "c_api.h"
#include "global.h"
#include "util.h"
#include "myepoll.h"
#include "watchdog.h"
#include "vfs_maintain.h"
#include "vfs_tmp_status.h"

#include <sys/sendfile.h>
#include <stdio.h>
/*
 *临时文件只存放任务信息，不存放协议报文头，在扫描临时目录时，如果有文件可用，需要设置协议头数据到发送缓冲区
 */

extern atomic_t taskcount[TASK_UNKNOWN];
#define MAXTMPFILE 10240
#define PREFIX  "voss_"
#define PREFIX_LEN 5
#define SUFFIX  ".data"
const char *s_server_stat[STAT_MAX] = {"UNKOWN_STAT", "WAIT_SYNC", "SYNCING", "ON_LINE", "OFFLINE"};

extern uint8_t self_stat;

static fileinfo *g_tmp_files = NULL;
static int g_tmp_index;

volatile extern int stop;		//1-服务器停止，0-服务器运行中

extern t_ip_info self_ipinfo;
int vfs_agent_log = -1;
struct conn defconn;
struct mybuff databuff;  /*存放从TASK_FIN队列获取的任务信息数据*/
/*在链路空闲时，直接发送心跳包*/

void get_data_from_task(t_vfs_tasklist *task)
{
	char buf[1024] = {0x0};
	t_task_base *base = &(task->task.base);
	char ip[16] = {0x0};
	ip2str(ip, base->dstip);
	time_t cur = time(NULL);
	if (cur - base->starttime > g_config.task_timeout)
		base->overstatus = OVER_TIMEOUT;

	if (task->status == TASK_CLEAN)
	{
		t_task_sub *sub = &(task->task.sub);
		size_t n = snprintf(buf, sizeof(buf), "%s:%s:%s:%c:%s:%s:%ld:%ld:%ld:%ld:%s:%d:%ld\n", base->src_domain, base->filename, ip, base->type, task_status[task->status%TASK_UNKNOWN], over_status[base->overstatus%OVER_LAST], sub->starttime, sub->endtime, base->fsize, base->ctime, base->filemd5, sub->need_sync, base->mtime);
		mybuff_setdata(&databuff, buf, n);
		LOG(vfs_agent_log, LOG_DEBUG, "get task[%s]\n", buf);
	}

	if (task->status == TASK_CLEAN || base->overstatus == OVER_TIMEOUT)
	{
		LOG(vfs_agent_log, LOG_DEBUG, "move task [%s:%s:%s:%s] to home\n", base->src_domain, base->filename, over_status[base->overstatus%OVER_LAST], task_status[task->status%TASK_UNKNOWN]);
		list_del_init(&(task->llist));
		if (task->status != TASK_CLEAN && task->task.user &&(ROLE_CS == self_ipinfo.role || ROLE_TRACKER == self_ipinfo.role))
		{
			t_tmp_status *tmp = task->task.user;
			set_tmp_blank(tmp->pos, tmp);
			task->task.user = NULL;
		}
		atomic_dec(&(taskcount[task->status]));
		memset(&(task->task), 0, sizeof(task->task));
		vfs_set_task(task, TASK_HOME);
	}
}

static void do_scan(struct conn *curcon, int *outdata, int type)
{
	LOG(vfs_agent_log, LOG_DEBUG, "task_timeout = %ld\n", g_config.task_timeout);
	scan_some_status_task(TASK_CLEAN, get_data_from_task);
	if (self_ipinfo.role == ROLE_CS)
		scan_some_status_task(TASK_SYNC_VOSS, get_data_from_task);

	char *data;
	size_t len;
	if (mybuff_getdata(&databuff, &data, &len) == 0)
	{
		if (type)
		{
			t_head_info head;
			memset(&head, 0, sizeof(head));
			create_voss_head((char *)&head, REQ_SUBMIT, len);
			mybuff_setdata(&(curcon->send_buff), (char *)&head, sizeof(head));
		}
		mybuff_setdata(&(curcon->send_buff), data, len);
		mybuff_skipdata(&databuff, len);
		*outdata |= 1;
	}
}

static void dump_to_file(struct conn *curcon, char *tmpdir)
{
	char day[16] = {0x0};
	get_strtime(day);
	char filename[256] = {0x0};
	snprintf(filename, sizeof(filename), "%s/%s%.10s%s", tmpdir, PREFIX, day, SUFFIX);
	int fd = open(filename, O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE, 0644);
	if (fd < 0)
	{
		LOG(vfs_agent_log, LOG_ERROR, "open %s err %m\n", filename);
		return;
	}

	char *data;
	size_t len;
	if (mybuff_getdata(&(curcon->send_buff), &data, &len) == 0)
	{
		size_t n = write(fd, data, len);
		if (n != len)
			LOG(vfs_agent_log, LOG_ERROR, "write %s err [%ld:%ld] %m\n", filename, len, n);
		if (n > 0)
			mybuff_skipdata(&(curcon->send_buff), n);
	}
	close(fd);
}

static int sortmtime(const void *p1, const void *p2)
{
	fileinfo *s1 = (fileinfo *)p1;
	fileinfo *s2 = (fileinfo *)p2;

	return s2->mtime > s1->mtime;
}

static void get_most_old_file(struct conn *curcon, int *outdata)
{
	int lfd;
	off_t start;
	size_t len;
	if (mybuff_getfile(&(curcon->send_buff), &lfd, &start, &len) == 0)
	{
		*outdata = FILE_DATA;
		LOG(vfs_agent_log, LOG_ERROR, "anything wrong :%s:%s:%d\n", ID, FUNC, LN);
		return;
	}
	if (g_tmp_index < 0)
		return;
	char *file = g_tmp_files[g_tmp_index].file;

	t_head_info head;
	struct stat st;
	int fd = open(file, O_RDONLY);
	if(fd > 0) 
	{
		fstat(fd, &st);
		memset(&head, 0, sizeof(head));
		create_voss_head((char *)&head, REQ_SUBMIT, st.st_size);
	}
	else 
	{
		LOG(vfs_agent_log, LOG_ERROR, "open %s error %m!\n", file);
		return ;
	}
	mybuff_setdata(&(curcon->send_buff), (char *)&head, sizeof(head));
	mybuff_setfile(&(curcon->send_buff), fd, 0, st.st_size);
	LOG(vfs_agent_log, LOG_DEBUG, "try send [%s] [%d]\n", file, st.st_size);
	*outdata = FILE_DATA;
}

static void rm_last_send_file()
{
	if (g_tmp_index < 0)
	{
		LOG(vfs_agent_log, LOG_ERROR, "anything wrong ? [%d]\n", g_tmp_index);
		return;
	}

	char *file = g_tmp_files[g_tmp_index].file;
	if (unlink(file))
		LOG(vfs_agent_log, LOG_ERROR, "unlink file %s err %m\n", file);
	g_tmp_index--;
}

static void scantmpdir(char *tmpdir, struct conn *curcon) 
{
	g_tmp_index = 0;
	if (!g_tmp_files)
	{
		g_tmp_files = (fileinfo *) malloc (sizeof(fileinfo) * MAXTMPFILE);
		if (g_tmp_files == NULL)
		{
			LOG(vfs_agent_log, LOG_ERROR, "malloc error %m\n");
			abort();
		}
	}
	memset(g_tmp_files, 0, sizeof(g_tmp_files));

	DIR *dp;
	struct dirent *dirp;
	char file[256] = {0x0};

	if ((dp = opendir(tmpdir)) == NULL) 
	{
		LOG(vfs_agent_log, LOG_ERROR, "opendir %s error %m!\n", tmpdir);
		return ;
	}

	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;
		if (strncmp(dirp->d_name, PREFIX, 5))
			continue;
		snprintf(file, sizeof(file), "%s/%s", tmpdir, dirp->d_name);
		struct stat filestat;
		if (stat(file, &filestat))
		{
			LOG(vfs_agent_log, LOG_ERROR, "stat %s error %m!\n", file);
			continue;
		}
		if (filestat.st_size < 10)
		{
			unlink(file);
			continue;
		}
		snprintf(g_tmp_files[g_tmp_index].file, 256, "%s", file);
		g_tmp_files[g_tmp_index].mtime = filestat.st_mtime;
		g_tmp_index++;
		if (g_tmp_index >= MAXTMPFILE)
		{
			LOG(vfs_agent_log, LOG_ERROR, "too many tmp file!\n");
			break;
		}
	}
	closedir(dp);
	if (g_tmp_index == 0)
	{
		g_tmp_index = -1;
		return;
	}
	qsort(g_tmp_files, g_tmp_index, sizeof(fileinfo), sortmtime);
	if (g_tmp_index)
		g_tmp_index--;
}

static int do_send(int fd)
{
	int ret = SOCK_COMP;
	int n = 0;
	struct conn *curcon = &acon[fd];
	int lfd;
	off_t start;
	char* data;
	size_t len;
	if(!mybuff_getdata(&(curcon->send_buff), &data, &len)) 
	{
		LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] get len from data [%d]\n", fd, len);
		while (1)
		{
			n = send(fd, data, len, MSG_DONTWAIT | MSG_NOSIGNAL);
			if(n > 0) 
			{
				LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] send len %d, datalen %d\n", fd, n, len);
				mybuff_skipdata(&(curcon->send_buff), n);
				if (n < len)
					ret = SOCK_SEND;
			}
			else if(errno == EINTR) 
				continue;
			else if(errno == EAGAIN) 
				ret = SOCK_SEND;
			else 
			{
				LOG(vfs_agent_log, LOG_ERROR, "%s:%s:%d fd[%d] send err %d:%d:%m\n", ID, FUNC, LN, fd, n, len);
				return SOCK_CLOSE;
			}
			break;
		}
	}
	if(ret == SOCK_COMP && !mybuff_getfile(&(curcon->send_buff), &lfd, &start, &len))
	{
		LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] get len from file [%d]\n", fd, len);
		size_t len1 = len > GSIZE ? GSIZE : len;
		while (1)
		{
			n = sendfile64(fd, lfd, &start, len1);
			if(n > 0) 
			{
				mybuff_skipfile(&(curcon->send_buff), n);
				LOG(vfs_agent_log, LOG_DEBUG, "%s:%s:%d fd[%d] send len %d, datalen %d\n", ID, FUNC, LN, fd, n, len1);
				if(n < len) 
					ret = SOCK_SEND_FILE;
				else
					rm_last_send_file();
			}
			else if(errno == EINTR) 
				continue;
			else if(errno == EAGAIN) 
				ret = SOCK_SEND_FILE;
			else 
			{
				LOG(vfs_agent_log, LOG_ERROR, "%s:%s:%d fd[%d] send err %d:%d:%m\n", ID, FUNC, LN, fd, n, len);
				return SOCK_CLOSE;
			}
			break;
		}
	}
	LOG(vfs_agent_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	return ret;
}

static int do_recv(int fd)
{
	struct conn *curcon = &acon[fd];

	char iobuf[2048];
	int n = -1;
	while (1)
	{
		n = recv(fd, iobuf, sizeof(iobuf), MSG_DONTWAIT);
		if (n > 0)
		{
			LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] recv len %d\n", fd, n);
			mybuff_setdata(&(curcon->recv_buff), iobuf, n);
			if (n == sizeof(iobuf))
			{
				LOG(vfs_agent_log, LOG_TRACE, "fd[%d] need recv nextloop %d\n", fd, n);
				continue;
			}
			return SOCK_COMP;
		}
		if (n == 0)
		{
			LOG(vfs_agent_log, LOG_ERROR, "fd[%d] close %s:%d!\n", fd, ID, LN);
			return SOCK_CLOSE;
		}
		if (errno == EINTR)
		{
			LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] need recv again!\n", fd);
			continue;
		}
		if (errno == EAGAIN)
		{
			LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] need recv next!\n", fd);
			return SOCK_COMP;
		}
		else
		{
			LOG(vfs_agent_log, LOG_ERROR, "fd[%d] close %s:%d!\n", fd, __FILE__, __LINE__);
			return SOCK_CLOSE;
		}
	}
	return SOCK_COMP;
}

static int do_update(int fd, t_head_info *h, char *data)
{
	return 0;
}

static int do_sync_file(int fd, t_head_info *h, char *data)
{
	return 0;
}

static int do_sync_dir(int fd, t_head_info *h, char *data)
{
	return 0;
}

static int set_from_voss(int fd, t_head_info *h, char *data)
{
	if (h->cmdid == REQ_VFS_CMD)
	{
		do_request(fd, h->totallen - HEADSIZE, data + HEADSIZE);
		return 0;
	}

	if (h->cmdid == REQ_CONF_UPDATE)
		return do_update(fd, h, data);
	else if (h->cmdid == REQ_SYNC_FILE)
		return do_sync_file(fd, h, data);
	else if (h->cmdid == REQ_SYNC_DIR)
		return do_sync_dir(fd, h, data);
	return 0;
}

static int do_hb(int fd, int *havedata)
{
	t_head_info head;
	struct conn *curcon = &acon[fd];
	create_voss_head((char *)&head, REQ_HEARTBEAT, sizeof(self_stat));
	char buf[32] = {0x0};
	memcpy(buf, &head, sizeof(head));
	memcpy(buf + sizeof(head), &self_stat, sizeof(self_stat));
	mybuff_setdata(&(curcon->send_buff), buf, sizeof(head) + sizeof(self_stat));
	*havedata |= 1;
	LOG(vfs_agent_log, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);
	return 0;
}

static int do_login(int fd, int *havedata)
{
	struct conn *curcon = &acon[fd];
	int outlen;
	char outbuf[1024] = {0x0};
	create_msg(outbuf, &outlen, REQ_AUTH, self_ipinfo.sip, strlen(self_ipinfo.sip));
	mybuff_setdata(&(curcon->send_buff), outbuf, outlen);
	*havedata |= 1;
	return 0;
}

static int check_req(int fd, int *o)
{
	t_head_info h;
	t_body_info b;
	memset(&b, 0, sizeof(b));
	memset(&h, 0, sizeof(h));
	char *data;
	size_t datalen;
	if (get_client_data(fd, &data, &datalen))
	{
		LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] no data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	int ret = parse_msg(data, datalen, &h);
	if (ret == E_NOT_SUFFIC)
	{
		LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] no suffic data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	if (datalen < h.totallen)
	{
		LOG(vfs_agent_log, LOG_DEBUG, "fd[%d] no suffic data!\n", fd);
		return -1;  /*no suffic data, need to get data more */
	}
	if (h.cmdid == REQ_STOPVFS)
	{
		LOG(vfs_agent_log, LOG_FAULT, "recv REQ_STOPVFS!!!\n");
		stop = 1;
		return -1;  /*no suffic data, need to get data more */
	}
	int clen = h.totallen;
	ret = set_from_voss(fd, &h, data);
	consume_client_data(fd, clen);
	if (*o < 1)
		*o = 1;
	return ret;
}

static int agent_log_init() 
{
	char *logname = myconfig_get_value("log_agent_logname");
	if (!logname)
		logname = "./agent_log.log";

	char *cloglevel = myconfig_get_value("log_agent_loglevel");
	int loglevel = LOG_NORMAL;
	if (cloglevel)
		loglevel = getloglevel(cloglevel);
	int logsize = myconfig_get_intval("log_agent_logsize", 100);
	int logintval = myconfig_get_intval("log_agent_logtime", 3600);
	int lognum = myconfig_get_intval("log_agent_lognum", 10);
	vfs_agent_log = registerlog(logname, loglevel, logsize, logintval, lognum);
	if (vfs_agent_log < 0)
		return -1;
	LOG(vfs_agent_log, LOG_DEBUG, "vfs_agent init log ok!\n");
	mybuff_init(&databuff);
	return 0;
}

static void do_run(struct threadstat *thst)
{
	int timespan = myconfig_get_intval("voss_timesplice", 60);
	int heartbeat = myconfig_get_intval("voss_heartbeat", 30);
	char* tmpdir = myconfig_get_value("voss_tmpdir");
	if (tmpdir == NULL)
		tmpdir = "../path/tmpdir";

	int i = 0;
	int fd = -1;
	char *serverip = NULL;
	int serverport = g_config.sig_port;
	for(i = 0; (serverip = myconfig_get_multivalue( "voss_server_ip", i)) != NULL; i++)
	{
		uint32_t uip = str2ip((char const*)serverip);
		char tmp[256] = {0x0};
		if(uip == INADDR_NONE)
		{
			LOG(vfs_agent_log, LOG_DEBUG, "ip %s is domain!\n", serverip);
			memset(tmp, 0 , sizeof(tmp));
			if(get_ip_by_domain(tmp, serverip))
			{
				LOG(vfs_agent_log, LOG_ERROR, "get_ip_by_domain %s err %m\n", serverip);
				continue;
			}
			serverip = tmp;
		}
		fd = createsocket(serverip, serverport);
		if(fd < 0)
		{
			char val[256] = {0x0};
			snprintf(val, sizeof(val), "%s connect %s:%d err %m\n", self_ipinfo.sip, serverip, serverport);
			SetStr(VFS_STR_CONNECT_E, val);
			LOG(vfs_agent_log, LOG_ERROR, "%s", val);
			continue;
		}
		break;
	}

	if (fd < 0)
	{
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "%s connect %s:%d err %m\n", self_ipinfo.sip, serverip, serverport);
		SetStr(VFS_STR_CONNECT_E, val);
		LOG(vfs_agent_log, LOG_ERROR, "create socket err %m\n");
		/*连接不上服务端时，为了确保vfs系统正常，需要清理回收资源 ，下个周期重新连接服务端 */
		/*将临时信息写的文件系统，连接上服务端后，再上报该数据*/
	}
	else
		LOG(vfs_agent_log, LOG_DEBUG, "create socket ok fd [%d]\n", fd);

	int outdata = 0;
	struct conn *curcon = NULL;
	if (fd >= 0)
	{
		fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
		curcon = &(acon[fd]);
	}
	else
		curcon = &(defconn);
	mybuff_reinit(&(curcon->send_buff));
	mybuff_reinit(&(curcon->recv_buff));
	if (fd < 0)
	{
		do_scan(curcon, &outdata, 0);
		if (outdata)
			dump_to_file(curcon, tmpdir);
		return ;
	}

	do_login(fd, &outdata);
	fd_set rfds;
	fd_set wfds;
	scantmpdir(tmpdir, curcon); 
	struct timeval tv;
	int ret = 0;
	time_t now = time(NULL);
	time_t last = now;
	LOG(vfs_agent_log, LOG_DEBUG, "enter select loop!\n");
	while (!stop)
	{
		//send old tmp file
		get_most_old_file(curcon, &outdata);
		thread_reached(thst);

		//send current task info, only when tmp file had send finish
		if (outdata != 2)
		{
			LOG(vfs_agent_log, LOG_DEBUG, "start scan task\n");
			do_scan(curcon, &outdata, 1);
			//send hb
			if (now - last > heartbeat)
			{
				last = now;
				do_hb(fd, &outdata);
			}
		}

		FD_ZERO( &rfds );
		FD_SET( fd, &rfds );
		if (outdata)
		{
			FD_ZERO( &wfds );
			FD_SET( fd, &wfds );
		}
		tv.tv_sec = timespan;
		tv.tv_usec = 0;

		now = time(NULL);

		if (outdata)
			ret = select( fd + 1, &rfds, &wfds, NULL, &tv);
		else
			ret = select( fd + 1, &rfds, NULL, NULL, &tv);
		if (ret  == -1)
        {
			LOG(vfs_agent_log, LOG_ERROR, "select err %m\n");
			break;
        }

		if (ret == 0)
		{
			if (outdata == 2)
				LOG(vfs_agent_log, LOG_ERROR, "sendfile too long\n");
			continue;
		}

		outdata = 0;

        if( FD_ISSET( fd, &wfds ) )
        {
			ret = do_send(fd);
			if (ret == SOCK_CLOSE)
				break;
			if (ret == SOCK_SEND)
				outdata = 1;
			if (ret == SOCK_SEND_FILE)
				outdata = 2;
        }

        if( FD_ISSET( fd, &rfds ) )
        {
			ret = do_recv(fd);
			if (ret == SOCK_CLOSE)
				break;
			while (1)
			{
				int ret = check_req(fd, &outdata);
				if (ret == -1)
					break;
				if (ret == SOCK_CLOSE)
				{
					LOG(vfs_agent_log, LOG_ERROR, "fd [%d] close\n", fd);
					close(fd);
					return;
				}
			}
        }
	}
	close(fd);
	return;
}

int vfs_agent_thread(void *arg)
{
	struct threadstat *thst = get_threadstat();
	if (agent_log_init())
	{
		fprintf(stderr, "agent_log_init err %m\n");
		stop = 1;
		return -1;
	}
	while (!stop)
	{
		thread_reached(thst);
		do_run(thst);
		sleep(10);
	}
	return 0;
}

int init_vfs_agent()
{
	int iret = 0;
	if((iret = register_thread("vfs_agent", vfs_agent_thread, NULL)) < 0)
		return iret;
	return 0;
}

