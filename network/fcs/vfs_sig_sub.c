/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"
#include "vfs_localfile.h"

int active_send(vfs_fcs_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %x\n", peer->fd, h->cmdid);
	char obuf[2048] = {0x0};
	int n = 0;
	peer->hbtime = time(NULL);
	n = create_sig_msg(h->cmdid, h->status, b, obuf, h->bodylen);
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	return 0;
}

void do_sync_del_req(int fd, t_vfs_sig_body *b)
{
	t_vfs_sync_task * task = (t_vfs_sync_task *)b;
	char obuf[2048] = {0x0};
	int n = 0;
	t_task_base base;
	memset(&base, 0, sizeof(base));
	snprintf(base.src_domain, sizeof(base.src_domain), "%s", task->domain);
	base.type = TASK_DELFILE;
	int next = find_last_index(&base, task->starttime);
	if (next == -1)
		LOG(vfs_sig_log, LOG_NORMAL, "no more del file [%s] [%s]\n", task->domain, ctime(&(task->starttime)));
	else
	{
		time_t cur = time(NULL);
		while(1) 
		{
			memset(obuf, 0, sizeof(obuf));
			n = create_sig_msg(NEWTASK_REQ, TASK_SYNC_DIR, (t_vfs_sig_body *)&base, obuf, sizeof(t_task_base));
			set_client_data(fd, obuf, n);
			next++;
			memset(base.filename, 0, sizeof(base.filename));
			next = get_from_del_file (&base, next, cur);
			if (next == -1)
				break;
		}
	}
	memset(obuf, 0, sizeof(obuf));
	n = create_sig_msg(SYNC_DEL_RSP, TASK_SYNC_DIR, b, obuf, sizeof(t_vfs_sync_task));
	set_client_data(fd, obuf, n);
	modify_fd_event(fd, EPOLLOUT);
}

void do_sync_dir_req(int fd, t_vfs_sig_body *b)
{
	t_vfs_tasklist *task = NULL;
	char obuf[2048] = {0x0};
	int n = 0;
	if (vfs_get_task(&task, TASK_HOME))
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] do_sync_dir_req ERROR!\n", fd);
		memset(obuf, 0, sizeof(obuf));
		n = create_sig_msg(SYNC_DIR_RSP, TASK_SYNC_DIR, b, obuf, sizeof(t_vfs_sync_task));
		set_client_data(fd, obuf, n);
		modify_fd_event(fd, EPOLLOUT);
		return;
	}
	t_task_base *base = &(task->task.base);
	memset(base, 0, sizeof(t_task_base));
	memcpy(base, b, sizeof(t_vfs_sync_task));
	base->dstip = getpeerip(fd);
	vfs_set_task(task, TASK_Q_SYNC_DIR_REQ);
}

static int do_newtask(int fd, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	t_task_base *base = (t_task_base*) b;
	base->starttime = time(NULL);
	base->dstip = str2ip(self_ipinfo.s_ip);

	struct conn *curcon = &acon[fd];
	vfs_fcs_peer *peer = (vfs_fcs_peer *) curcon->user;

	t_task_sub sub;
	memset(&sub, 0, sizeof(sub));
	sub.processlen = base->fsize;
	sub.starttime = time(NULL);
	sub.lasttime = sub.starttime;
	sub.oper_type = OPER_GET_REQ;
	if (peer->role != ROLE_CS || base->type == TASK_DELFILE)
	{
		LOG(vfs_sig_log, LOG_ERROR, "what happen %s %s %c\n", base->src_domain, base->filename, base->type);
		return -1;
	}

	if (check_localfile_md5(base, VIDEOFILE) == LOCALFILE_OK)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s check md5 ok!\n", base->filename, base->src_domain);
		return 0;
	}

	sub.need_sync = TASK_SYNC_ISDIR;
	memset(sub.peerip, 0, sizeof(sub.peerip));
	ip2str(sub.peerip, peer->ip);

	if (check_task_from_alltask(base, &sub) == 0)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s task exist!\n", base->filename, base->src_domain);
		return 0;
	}

	t_vfs_tasklist *task = NULL;
	if (vfs_get_task(&task, TASK_HOME))
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] do_newtask ERROR!\n", fd);
		h->status = TASK_FAILED;
		return -1;
	}

	memset(&(task->task), 0, sizeof(task->task));
	memcpy(&(task->task.base), base, sizeof(t_task_base));
	memcpy(&(task->task.sub), &sub, sizeof(t_task_sub));

	task->task.base.overstatus = OVER_UNKNOWN;
	sync_para.total_sync_task++;
	vfs_set_task(task, TASK_WAIT);  
	add_task_to_alltask(task);
	add_to_sync_dir_list(base->filename);
	return 0;
}

static int check_count_one_day()
{
	static time_t last = 0;
	static int count = 0;
	if (last == 0)
		last = time(NULL);
	if (count < g_config.sync_dir_count)
	{
		count++;
		return 0;
	}
	if (time(NULL) - last >= 86400)
	{
		last = time(NULL);
		count = 1;
		return 0;
	}
	return -1;
}

static int do_req(int fd, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	struct conn *curcon = &acon[fd];
	char obuf[2048] = {0x0};
	int n = 0;
	t_vfs_sig_body ob;
	memset(&ob, 0, sizeof(ob));
	int bodylen = 0;
	vfs_fcs_peer *peer = (vfs_fcs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	bodylen = sizeof(self_stat);
	memcpy(ob.body, &self_stat, sizeof(self_stat));

	switch(h->cmdid)
	{
		case HEARTBEAT_REQ:
			n = create_sig_msg(HEARTBEAT_RSP, h->status, &ob, obuf, bodylen);
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			if (h->bodylen != 1)
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad hb , no SERVER_STAT\n", fd);
			else
				peer->server_stat = *(b->body);
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a HB\n", fd);
			return RECV_SEND;		

		case ADDONE_REQ:
			if (init_cfg_connect(b->body, peer))
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] init_cfg_connect err !\n", fd);
				return RECV_CLOSE;
			}
			bodylen = strlen(self_ipinfo.sip);
			memcpy(ob.body, self_ipinfo.sip, bodylen);
			n = create_sig_msg(ADDONE_RSP, h->status, &ob, obuf, bodylen);
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a ADDONE_REQ\n", fd);
			return RECV_SEND;

		case NEWTASK_REQ:
			if (h->bodylen != sizeof(t_task_base))
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad NEWTASK_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			do_newtask(fd, h, b);
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a NEWTASK_REQ\n", fd);
			return RECV_ADD_EPOLLIN;
		
		case NEWTASK_RSP:
			if (h->bodylen != sizeof(t_task_base))
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad %s bodylen[%d]!\n", fd, str_cmd[h->cmdid], h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			t_task_base *base = (t_task_base *)b;
			t_vfs_tasklist *task = NULL;
			t_task_sub sub;
			memset(&sub, 0, sizeof(sub));
			if (get_task_from_alltask(&task, base, &sub))
			{
				LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] recv a %s task rsp, have been process!\n", fd, base->filename);
				return RECV_ADD_EPOLLIN;
			}
			if (base->type == TASK_ADDFILE)
				set_fcs_time_stamp(&(task->task.base));
			task->task.base.overstatus = h->status;
			list_del_init(&(task->userlist));
			vfs_set_task(task, TASK_CLEAN);
			peer->taskcount--;
			if (peer->taskcount < 0)
				peer->taskcount = 0;
			LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] recv %s rsp status [%s]!\n", fd, base->filename, over_status[h->status%OVER_LAST]);
			return RECV_ADD_EPOLLIN;

		case SYNC_DIR_REQ:
			if (h->bodylen != sizeof(t_vfs_sync_task))
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad SYNC_DIR_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DIR_REQ\n", fd);
			if (check_count_one_day())
			{
				LOG(vfs_sig_log, LOG_DEBUG, "too many SYNC_DIR_REQ! close it!\n");
				return RECV_CLOSE;
			}
			do_sync_dir_req(fd, b);
			return RECV_ADD_EPOLLOUT;

		case SYNC_DIR_RSP:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DIR_RSP\n", fd);
			return RECV_ADD_EPOLLIN;

		case SYNC_DEL_REQ:
			if (h->bodylen != sizeof(t_vfs_sync_task))
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad SYNC_DEL_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DEL_REQ\n", fd);
			do_sync_del_req(fd, b);
			return RECV_ADD_EPOLLOUT;

		default:
			LOG(vfs_sig_log, LOG_ERROR, "fd[%d] recv a bad cmd [%x] status[%x]!\n", fd, h->cmdid, h->status);
		
	}
	return RECV_ADD_EPOLLIN;
}
