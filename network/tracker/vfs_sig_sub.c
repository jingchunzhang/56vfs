/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"
void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_WAIT_SYNC);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(task, TASK_WAIT);
	}

	int once = 0;
	while (1)
	{
		if (once >= g_config.cs_max_task_run_once)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "too many task in tracker %d %d\n", once, g_config.cs_max_task_run_once);
			break;
		}
		ret = vfs_get_task(&task, TASK_WAIT);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		t_task_base *base = &(task->task.base);
		t_task_sub *sub = &(task->task.sub);
		if(check_task_from_alltask(base, sub) == 0)
			LOG(vfs_sig_log, LOG_DEBUG, "file %s isp [%d] is running\n", base->filename, sub->isp);
		once++;
		if (do_dispatch(task))
			vfs_set_task(task, TASK_WAIT_SYNC);
		else
		{
			vfs_set_task(task, TASK_RUN);
			add_task_to_alltask(task);
		}
	}
}

int active_send(vfs_tracker_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	LOG(vfs_sig_log, LOG_DEBUG, "send %d cmdid %x\n", peer->fd, h->cmdid);
	char obuf[2048] = {0x0};
	size_t n = 0;
	peer->hbtime = time(NULL);
	n = create_sig_msg(h->cmdid, h->status, b, obuf, h->bodylen);
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	return 0;
}

static int do_req(int fd, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	struct conn *curcon = &acon[fd];
	vfs_tracker_peer *peer = (vfs_tracker_peer *) curcon->user;
	char obuf[2048] = {0x0};
	size_t n = 0;
	t_vfs_sig_body ob;
	memset(&ob, 0, sizeof(ob));
	int bodylen = 0;
	char ip[16] = {0x0};
	ip2str(ip, peer->ip);
	peer->hbtime = time(NULL);
	bodylen = sizeof(self_stat);
	memcpy(ob.body, &self_stat, sizeof(self_stat));
	switch(h->cmdid)
	{
		case HEARTBEAT_REQ:
			if (h->status != HB_C_2_T && HB_T_2_T != h->status)
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad hb status[%x]!\n", fd, h->status);
				return RECV_ADD_EPOLLIN;
			}
			n = create_sig_msg(HEARTBEAT_RSP, h->status, &ob, obuf, bodylen);
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			if (h->bodylen != 1)
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad hb , no SERVER_STAT\n", fd);
			else
				peer->server_stat = *(b->body);
			LOG(vfs_sig_log, LOG_TRACE, "fd[%d] recv a HB\n", fd);
			return RECV_ADD_EPOLLOUT;

		case HEARTBEAT_RSP:
			peer->sock_stat = IDLE;
			if (h->bodylen != 1)
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad hb rsp, no SERVER_STAT\n", fd);
			else
				peer->server_stat = *(b->body);
			LOG(vfs_sig_log, LOG_TRACE, "fd[%d] recv a HB RSP\n", fd);
			return RECV_ADD_EPOLLIN;

		case ADDONE_REQ:
			if (init_cfg_connect(b->body, peer))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] init_cfg_connect err !\n", fd);
				return RECV_CLOSE;
			}
			bodylen = strlen(self_ipinfo.sip);
			memcpy(ob.body, self_ipinfo.sip, bodylen);
			n = create_sig_msg(ADDONE_RSP, h->status, &ob, obuf, bodylen);
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a ADDONE_REQ\n", fd);
			return RECV_ADD_EPOLLOUT;

		case ADDONE_RSP:
			if (init_cfg_connect(b->body, peer))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] init_cfg_connect err !\n", fd);
				return RECV_CLOSE;
			}
			peer->sock_stat = IDLE;
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a ADDONE_RSP\n", fd);
			return RECV_ADD_EPOLLIN;

		case TASKINFO_REQ:
			if (h->status != TASKINFO_C_2_T)
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad TASKINFO_REQ status[%x]!\n", fd, h->status);
				return RECV_ADD_EPOLLIN;
			}
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a TASKINFO_REQ\n", fd);
			return RECV_ADD_EPOLLOUT;

		case NEWTASK_REQ:
			if (h->status != TASK_DISPATCH )
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad NEWTASK_REQ status[%x]!\n", fd, h->status);
				return RECV_ADD_EPOLLIN;
			}
			if (h->bodylen != sizeof(t_task_base))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad %s bodylen[%d]!\n", fd, str_cmd[h->cmdid], h->bodylen);
				return RECV_CLOSE;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a NEWTASK_REQ\n", fd);
			if (do_newtask(fd, h, b))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] do_newtask error!\n", fd);
				n = create_sig_msg(NEWTASK_RSP, TASK_FAILED, b, obuf, sizeof(t_task_base));
				set_client_data(fd, obuf, n);
				peer->sock_stat = SEND_LAST;
				return RECV_ADD_EPOLLOUT;
			}
			return RECV_ADD_EPOLLIN;

		case NEWTASK_RSP:
			if (h->bodylen != sizeof(t_task_base))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv %s a bad bodylen[%d]!\n", fd, str_cmd[h->cmdid], h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a %s\n", fd, str_cmd[h->cmdid]);
			update_task(fd, h, b);
			return RECV_ADD_EPOLLIN;

		default:
			LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad cmd [%x] status[%x] len [%d] [%s]!\n", fd, h->cmdid, h->status, h->bodylen, ip);
			return RECV_CLOSE;

	}
	return RECV_ADD_EPOLLIN;
}
