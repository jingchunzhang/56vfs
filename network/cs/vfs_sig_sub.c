/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "c_api.h"

static uint32_t get_a_ok_ip(t_cs_dir_info * csinfos, int isp, int8_t *index)
{
	if (csinfos->index == 0)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "cs_dir index %d\n", csinfos->index);
		return 0;
	}
	if (*index >= csinfos->index)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "at the last index %d %d\n", csinfos->index, *index);
		return 0;
	}
	uint32_t ip = 0;
	int i= 0;
	vfs_cs_peer *peer;
	for (i = *index; i < csinfos->index; i++)
	{
		if (csinfos->isp[i] != isp)
			continue;
		if (check_self_ip(csinfos->ip[i]) == 0)
			continue;
		if (find_ip_stat(csinfos->ip[i], &peer))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip [%u] not active !\n", csinfos->ip[i]);
			continue;
		}
		ip = csinfos->ip[i];
		break;
	}
	*index = i++;
	return ip;
}

static void do_voss_sync_file(t_vfs_tasklist *task)
{
	vfs_set_task(task, TASK_WAIT);
	return ;
	t_task_base *base = (t_task_base*) &(task->task.base);
	t_task_sub *sub = (t_task_sub*) &(task->task.sub);
	if(base->retry == -1)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "sync %s ERROR!\n", base->filename);
		vfs_set_task(task, TASK_CLEAN);
		return;
	}
	LOG(vfs_sig_log, LOG_DEBUG, "sync %s retry %d!\n", base->filename, base->retry);
	t_cs_dir_info  cs;
	if(get_cs_info_by_path(base->filename, &cs))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "get_cs_info_by_path %s ERROR!\n", base->filename);
		base->overstatus = OVER_SRC_DOMAIN_ERR;
		vfs_set_task(task, TASK_CLEAN);
		return;
	}
	uint32_t ip = get_a_ok_ip(&cs, base->isp, &(base->retry));
	if (ip == 0)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "can not get ok ip isp:%s:%s:%d next try src\n", ispname[base->isp], base->filename, base->retry);
		t_ip_info ipinfo0;
		t_ip_info *ipinfo = &ipinfo0;
		if (get_ip_info(&ipinfo, base->src_domain, 1))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "get_ip_info err ip %s\n", base->src_domain);
			vfs_set_task(task, TASK_CLEAN);
			return;
		}
		ip = ipinfo->ip;
		base->retry = -1;
	}
	ip2str(sub->peerip, ip);
	vfs_set_task(task, TASK_WAIT);
}

static void do_voss_sync_dir(t_vfs_tasklist *task)
{
	t_task_base *base = (t_task_base*) &(task->task.base);
	t_task_sub *sub = (t_task_sub*) &(task->task.sub);
	int d1 = atoi(base->filename);
	char *p = strchr(base->filename, '/');
	if (p == NULL)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "ERROR format %s\n", base->filename);
		base->overstatus = OVER_MALLOC;
		return ;
	}
	p++;
	int d2 = atoi(p);
	t_vfs_sync_list *vfs_sync;  
	vfs_sync = malloc(sizeof(t_vfs_sync_list));
	if (vfs_sync == NULL)
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "ERROR malloc %m\n");
		base->overstatus = OVER_MALLOC;
		return ;
	}
	memset(vfs_sync, 0, sizeof(t_vfs_sync_list));
	INIT_LIST_HEAD(&(vfs_sync->list));
	vfs_sync->sync_task.starttime = sub->starttime;
	vfs_sync->sync_task.endtime = time(NULL);
	vfs_sync->sync_task.d1 = d1;
	vfs_sync->sync_task.d2 = d2;
	vfs_sync->sync_task.type = TASK_ADDFILE;
	snprintf(vfs_sync->sync_task.domain, sizeof(vfs_sync->sync_task.domain), "%s", base->src_domain);
	LOG(vfs_sig_log, LOG_NORMAL, "gen sync task %d %d %s %s\n", d1, d2, vfs_sync->sync_task.domain, ctime(&base->starttime));
	list_add_head(&(vfs_sync->list), &sync_list);
	base->overstatus = OVER_OK;
	self_stat = WAIT_SYNC;
	sync_para.flag = 0;
}

static void do_check_sync_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_SYNC_VOSS);
		if (ret != GET_TASK_OK)
			break;
		if (task->task.base.type == TASK_ADDFILE)
			do_voss_sync_file(task);
		else if (task->task.base.type == TASK_SYNCDIR)
		{
			do_voss_sync_dir(task);
			vfs_set_task(task, TASK_CLEAN);
		}
		else
		{
			LOG(vfs_sig_log, LOG_ERROR, "err type in TASK_SYNC_VOSS [%c:%s]\n", task->task.base.type, task->task.base.filename);
			task->task.base.overstatus = OVER_E_TYPE;
			vfs_set_task(task, TASK_CLEAN);
		}
	}

	while (1)
	{
		ret = vfs_get_task(&task, TASK_Q_SYNC_DIR_RSP);
		if (ret != GET_TASK_OK)
			break;
		vfs_cs_peer *peer;
		if (find_ip_stat(task->task.base.dstip, &peer))
		{
			LOG(vfs_sig_log, LOG_ERROR, "find_ip_stat TASK_Q_SYNC_DIR_RSP error %u\n", task->task.base.dstip);
			vfs_set_task(task, TASK_HOME);
			continue;
		}
		if (task->task.base.overstatus == OVER_MALLOC)
		{
			char obuf[2048] = {0x0};
			int n = 0;
			n = create_sig_msg(SYNC_DIR_RSP, TASK_SYNC_DIR, (t_vfs_sig_body *)(&(task->task.base)), obuf, sizeof(t_vfs_sync_task));
			set_client_data(peer->fd, obuf, n);
			modify_fd_event(peer->fd, EPOLLOUT);
			vfs_set_task(task, TASK_HOME);
			continue;
		}
		char *o = task->task.user;
		if (o == NULL)
			LOG(vfs_sig_log, LOG_ERROR, "ERR task->task.user is null!\n");
		else
		{
			set_client_data(peer->fd, o, task->task.base.fsize);
			modify_fd_event(peer->fd, EPOLLOUT);
			free(task->task.user);
			task->task.user = NULL;
		}
		vfs_set_task(task, TASK_HOME);
	}
}

static int get_src_domain_ip(t_vfs_tasklist * task)
{
	char *domain = task->task.base.src_domain;
	char *file = task->task.base.filename;
	char *peerip = task->task.sub.peerip;
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	if(get_ip_info(&ipinfo, domain, 1))
	{
		LOG(vfs_sig_log, LOG_ERROR, "%s:%s: get_ip_info error!\n", file, domain);
		return -1;
	}
	if (strcmp(ipinfo->s_ip, peerip))
	{
		LOG(vfs_sig_log, LOG_NORMAL, "%s:%s:%s: try get from src!\n", file, domain, peerip);
		strcpy(peerip, ipinfo->s_ip);
		return 0;
	}
	return -1;
}

static void do_fin_task()
{
	while (1)
	{
		t_vfs_tasklist *task = NULL;
		if (vfs_get_task(&task, TASK_FIN))
			return ;
		dump_task_info ((char*)FUNC, LN, &(task->task.base));
		if (OVER_E_MD5 == task->task.base.overstatus)
		{
			if (g_config.retry && task->task.base.retry < g_config.retry)
			{
				task->task.base.retry++;
				task->task.base.overstatus = OVER_UNKNOWN;
				LOG(vfs_sig_log, LOG_NORMAL, "retry[%d:%d:%s]\n", task->task.base.retry, g_config.retry, task->task.base.filename);
				vfs_set_task(task, TASK_WAIT);
				continue;
			}
		}
		else if (OVER_E_OPEN_SRCFILE == task->task.base.overstatus)
		{
			if (g_config.retry && task->task.base.retry < g_config.retry)
			{
				if (get_src_domain_ip(task) == 0)
				{
					task->task.base.retry++;
					task->task.base.overstatus = OVER_UNKNOWN;
					LOG(vfs_sig_log, LOG_NORMAL, "retry[%d:%d:%s]\n", task->task.base.retry, g_config.retry, task->task.base.filename);
					vfs_set_task(task, TASK_WAIT);
					continue;
				}
			}
		}
		if (task->task.sub.need_sync == TASK_SOURCE)
		{
			if (task->task.base.overstatus == OVER_OK || task->task.base.overstatus == OVER_UNLINK)
				if (sync_task_2_group(task))
				{
					vfs_set_task(task, TASK_DELAY);
					continue;
				}
			do_task_rsp(task);
		}
		else if (task->task.sub.need_sync == TASK_SYNC_ISDIR)
		{
			sync_para.total_synced_task++;
			if (sync_para.flag == 2 && sync_para.total_synced_task >= sync_para.total_sync_task)
			{
				LOG(vfs_sig_log, LOG_NORMAL, "SYNC[%d:%d]\n", sync_para.total_synced_task, sync_para.total_sync_task);
				if (self_stat != OFF_LINE)
					self_stat = ON_LINE;
			}
		}
		if ((task->task.base.type == TASK_ADDFILE || TASK_LINKFILE == task->task.base.type) && task->task.base.overstatus == OVER_OK)
			set_cs_time_stamp(&(task->task.base));
		if (TASK_LINKFILE == task->task.base.type && task->task.base.overstatus == OVER_OK)
			localfile_link_task(&(task->task.base));
		if (task->task.user)
		{
			t_tmp_status *tmp = task->task.user;
			set_tmp_blank(tmp->pos, tmp);
			task->task.user = NULL;
		}
		t_vfs_tasklist *task0;
		get_task_from_alltask(&task0, &(task->task.base), &(task->task.sub));
		vfs_set_task(task, TASK_CLEAN);
	}
}

static void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_DELAY);
		if (ret != GET_TASK_OK)
			break;
		vfs_set_task(task, TASK_FIN);
	}

	int once = 0;
	while (1)
	{
		if (once >= g_config.cs_max_task_run_once)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "too many task in cs %d %d\n", once, g_config.cs_max_task_run_once);
			break;
		}
		ret = vfs_get_task(&task, TASK_WAIT_SYNC_IP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		once++;
		vfs_set_task(task, TASK_WAIT_SYNC);
	}

	once = 0;
	while (1)
	{
		if (once >= g_config.cs_max_task_run_once)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "too many task in cs %d %d\n", once, g_config.cs_max_task_run_once);
			break;
		}
		ret = vfs_get_task(&task, TASK_WAIT_SYNC);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		once++;
		uint32_t ip = str2ip(task->task.sub.peerip);
		vfs_cs_peer *peer = NULL;
		if (find_ip_stat(ip, &peer))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip[%u] not in active !\n", ip);
			t_ip_info ipinfo0;
			t_ip_info *ipinfo = &ipinfo0;
			if (get_ip_info(&ipinfo, task->task.sub.peerip, 1))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "get_ip_info err ip %s %s set to clean\n", task->task.sub.peerip, task->task.base.filename);
				task->task.base.overstatus = OVER_E_IP;
				vfs_set_task(task, TASK_FIN);  
			}
			else
				vfs_set_task(task, TASK_WAIT_SYNC_IP);
			continue;
		}
		do_dispatch_task(ip, task, peer);
		if (task->task.user)
		{
			t_tmp_status *tmp = task->task.user;
			set_tmp_blank(tmp->pos, tmp);
			task->task.user = NULL;
		}
		vfs_set_task(task, TASK_HOME);  /*同步信令 不作为任务上报*/
	}
	do_fin_task();
	do_check_sync_task();
}

int active_send(vfs_cs_peer *peer, t_vfs_sig_head *h, t_vfs_sig_body *b)
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
	char obuf[2048] = {0x0};
	size_t n = 0;
	t_vfs_sig_body ob;
	memset(&ob, 0, sizeof(ob));
	int bodylen = 0;
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
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
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad hb , no SERVER_STAT\n", fd);
			else
				peer->server_stat = *(b->body);
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a HB\n", fd);
			return RECV_ADD_EPOLLOUT;

		case HEARTBEAT_RSP:
			peer->sock_stat = IDLE;
			if (h->bodylen != 1)
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad hb rsp, no SERVER_STAT\n", fd);
			else
				peer->server_stat = *(b->body);
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a HB RSP\n", fd);
			return RECV_ADD_EPOLLIN;

		case ADDONE_REQ:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a ADDONE_REQ\n", fd);
			if (init_cfg_connect(b->body, peer))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] init_cfg_connect err !\n", fd);
				return RECV_CLOSE;
			}
			peer->hbtime = time(NULL) - g_config.timeout;
			peer->server_stat = h->status;
			bodylen = strlen(self_ipinfo.sip);
			memcpy(ob.body, self_ipinfo.sip, bodylen);
			n = create_sig_msg(ADDONE_RSP, self_stat, &ob, obuf, bodylen);
			set_client_data(fd, obuf, n);
			peer->sock_stat = SEND_LAST;
			return RECV_ADD_EPOLLOUT;

		case ADDONE_RSP:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a ADDONE_RSP [%d]\n", fd, h->bodylen);
			if (init_cfg_connect(b->body, peer))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] init_cfg_connect err !\n", fd);
				return RECV_CLOSE;
			}
			peer->server_stat = h->status;
			peer->hbtime = time(NULL) - g_config.timeout;
			peer->sock_stat = IDLE;
			return RECV_ADD_EPOLLIN;

		case TASKINFO_RSP:
			if (h->status != TASKINFO_T_2_C)
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad TASKINFO_RSP status[%x]!\n", fd, h->status);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a TASKINFO_RSP\n", fd);
			return RECV_ADD_EPOLLOUT;

		case NEWTASK_REQ:
			if (h->bodylen != sizeof(t_task_base))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad NEWTASK_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_CLOSE;
			}
			if (do_newtask(fd, h, b))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] do_newtask error!\n", fd);
				n = create_sig_msg(NEWTASK_RSP, h->status, b, obuf, sizeof(t_task_base));
				set_client_data(fd, obuf, n);
				peer->sock_stat = SEND_LAST;
				return RECV_ADD_EPOLLOUT;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a NEWTASK_REQ\n", fd);
			return RECV_ADD_EPOLLIN;

		case NEWTASK_RSP:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a NEWTASK_RSP\n", fd);
			return RECV_ADD_EPOLLIN;

		case SYNC_DIR_REQ:
			if (h->bodylen != sizeof(t_vfs_sync_task))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad SYNC_DIR_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DIR_REQ\n", fd);
			do_sync_dir_req(fd, b);
			return RECV_ADD_EPOLLOUT;

		case SYNC_DIR_RSP:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DIR_RSP\n", fd);
			if (peer->vfs_sync_list)
			{
				free(peer->vfs_sync_list);
				peer->vfs_sync_list = NULL;
			}
			sync_para.flag = 0;
			return RECV_ADD_EPOLLIN;

		case SYNC_DEL_REQ:
			if (h->bodylen != sizeof(t_vfs_sync_task))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad SYNC_DEL_REQ bodylen[%d]!\n", fd, h->bodylen);
				return RECV_ADD_EPOLLIN;
			}
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DEL_REQ\n", fd);
			do_sync_del_req(fd, b);
			return RECV_ADD_EPOLLOUT;

		case SYNC_DEL_RSP:
			LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] recv a SYNC_DEL_RSP\n", fd);
			if (peer->vfs_sync_list)
			{
				free(peer->vfs_sync_list);
				peer->vfs_sync_list = NULL;
			}
			sync_para.flag = 0;
			return RECV_ADD_EPOLLIN;

		default:
			LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] recv a bad cmd [%x] status[%x]!\n", fd, h->cmdid, h->status);
			return RECV_CLOSE;
	}
	return RECV_ADD_EPOLLIN;
}
