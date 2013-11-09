/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_sig.h"
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "vfs_localfile.h"
#include "vfs_del_file.h"
#include "vfs_tmp_status.h"
#include "vfs_sig.h"
#include "util.h"
#include "acl.h"

extern char hostname[64];
extern int vfs_sig_log;
extern int vfs_sig_log_err;
extern t_ip_info self_ipinfo;
extern t_sync_para sync_para;

void dump_task_info (char *from, int line, t_task_base *task)
{
	char ip[16] = {0x0};
	if (task->dstip)
	{
		ip2str(ip, task->dstip);
		LOG(vfs_sig_log, LOG_DEBUG, "from %s:%d filename [%s] srcdomain [%s] filesize[%ld] filemd5 [%s] filectime [%ld] type [%c] dstip[%s]\n", from, line, task->filename, task->src_domain, task->fsize, task->filemd5, task->ctime, task->type, ip);
	}
	else
		LOG(vfs_sig_log, LOG_DEBUG, "from %s:%d filename [%s] srcdomain [%s] filesize[%ld] filemd5 [%s] filectime [%ld] type [%c] dstip is null\n", from, line, task->filename, task->src_domain, task->fsize, task->filemd5, task->ctime, task->type);
}

int do_newtask(int fd, t_vfs_sig_head *h, t_vfs_sig_body *b)
{
	t_task_base *base = (t_task_base*) b;
	base->starttime = time(NULL);
	base->dstip = str2ip(self_ipinfo.sip);
	base->retry = 0;
	base->offsize = 0;
	dump_task_info ((char *) FUNC, LN, (t_task_base *)b);
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;

	t_task_sub sub;
	memset(&sub, 0, sizeof(sub));
	sub.processlen = base->fsize;
	sub.starttime = time(NULL);
	sub.lasttime = sub.starttime;
	sub.oper_type = OPER_GET_REQ;
	if (peer->role != ROLE_CS)
	{
		t_ip_info ipinfo0;
		t_ip_info *ipinfo = &ipinfo0;
		if (get_ip_info(&ipinfo, base->src_domain, 1))
		{
			char val[256] = {0x0};
			snprintf(val, sizeof(val), "get_ip_info %s err %m\n", base->src_domain);
			SetStr(VFS_GET_IP_ERR, val);
			LOG(vfs_sig_log_err, LOG_ERROR, "get_ip_info %s err %m\n", base->src_domain);
			h->status = TASK_FAILED;
			return -1;
		}
		snprintf(sub.peerip, sizeof(sub.peerip), "%s", ipinfo->s_ip);
	}
	else
	{
		if (self_stat == OFF_LINE)
			return 0;
	}

	t_vfs_tasklist *task = NULL;
	if (vfs_get_task(&task, TASK_HOME))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] do_newtask ERROR!\n", fd);
		h->status = TASK_FAILED;
		return -1;
	}
	task->task.user = NULL;

	LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] upip is [%u] filename [%s] role [%d]\n", fd, peer->ip, base->filename, peer->role);
	if (peer->role == ROLE_TRACKER)
	{
		sub.need_sync = TASK_SOURCE;
		task->upip = peer->ip;
		LOG(vfs_sig_log, LOG_DEBUG, "fd[%d] upip is [%u] filename [%s]\n", fd, task->upip, base->filename);
	}
	else if (peer->role == ROLE_CS)
	{
		memset(sub.peerip, 0, sizeof(sub.peerip));
		ip2str(sub.peerip, peer->ip);
		sub.need_sync = TASK_SRC_NOSYNC;
	}

	if (check_task_from_alltask(base, &sub) == 0)
		LOG(vfs_sig_log, LOG_DEBUG, "dup task filename [%s]\n", base->filename);

	if(h->status == TASK_SYNC_DIR)
	{
		sub.need_sync = TASK_SYNC_ISDIR;
		sync_para.total_sync_task++;
	}

	memset(&(task->task), 0, sizeof(task->task));
	memcpy(&(task->task.base), base, sizeof(t_task_base));
	memcpy(&(task->task.sub), &sub, sizeof(t_task_sub));

	dump_task_info ((char *) FUNC, LN, &(task->task.base));
	if (base->type == TASK_DELFILE)
	{
		if (delete_localfile(base))
			LOG(vfs_sig_log_err, LOG_ERROR, "%s:%s unlink %m!\n", base->filename, base->src_domain);
		else
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s unlink ok!\n", base->filename, base->src_domain);
		task->task.base.overstatus = OVER_OK;
		vfs_set_task(task, TASK_FIN);  
		return 0;
	}
	task->task.base.okindex = 1;
	if (check_localfile_md5(base, VIDEOFILE) == LOCALFILE_OK)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s check md5 ok!\n", base->filename, base->src_domain);
		task->task.base.overstatus = OVER_OK;
		vfs_set_task(task, TASK_FIN);  
		return 0;
	}
	if (base->type == TASK_LINKFILE)
	{
		t_task_base base0;
		memset(&base0, 0, sizeof(base0));
		snprintf(base0.filename, sizeof(base0.filename), "%s", base->linkfile);
		snprintf(base0.src_domain, sizeof(base0.src_domain), "%s", base->src_domain);
		if (check_localfile_md5(&base0, VIDEOFILE) == LOCALFILE_OK)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s check md5 ok!\n", base->linkfile, base->src_domain);
			task->task.base.overstatus = OVER_OK;
			task->task.base.okindex = 1;
			vfs_set_task(task, TASK_FIN);  
			return 0;
		}
	}
	task->task.base.overstatus = OVER_UNKNOWN;
	if(h->status != TASK_SYNC_DIR)
	{
		vfs_set_task(task, TASK_WAIT);
		set_task_to_tmp(task);
		add_task_to_alltask(task);
	}
	else
		vfs_set_task(task, TASK_Q_SYNC_DIR);
	return 0;
}
		
void do_task_rsp(t_vfs_tasklist *task)
{
	t_task_base *base = &(task->task.base);

	LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%d file[%s] finish\n", ID, FUNC, LN, task->task.base.filename);
	dump_task_info ((char *) FUNC, LN, base);
	vfs_cs_peer *peer = NULL;
	if (find_ip_stat(task->upip, &peer))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "file[%s] ip[%u] not online\n", task->task.base.filename, task->upip);
		return;
	}

	char obuf[2048] = {0x0};
	int n = create_sig_msg(NEWTASK_RSP, base->overstatus, (t_vfs_sig_body *)base, obuf, sizeof(t_task_base));
	set_client_data(peer->fd, obuf, n);
	peer->sock_stat = SEND_LAST;
	modify_fd_event(peer->fd, EPOLLOUT);
	dump_task_info ((char *) FUNC, LN, base);
}

int do_dispatch_task(uint32_t ip, t_vfs_tasklist *task, vfs_cs_peer *peer)
{
	char sip[16] = {0x0};
	ip2str(sip, ip);
	t_task_base *base = &(task->task.base);
	LOG(vfs_sig_log, LOG_NORMAL, "%s:%s:%d  dispatch [%s] to [%s]!\n", ID, FUNC, LN, base->filename, sip);
	char obuf[2048] = {0x0};
	int n = create_sig_msg(NEWTASK_REQ, TASK_SYNC, (t_vfs_sig_body *)base, obuf, sizeof(t_task_base));
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	return 0;
}
		
static void create_delay_task(uint32_t ip, t_task_base *base)
{
	t_vfs_tasklist *task = NULL;
	if (vfs_get_task(&task, TASK_HOME))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "[%s] do_newtask ERROR!\n", FUNC);
		return;
	}
	list_del_init(&(task->userlist));
	base->starttime = time(NULL);
	memset(&(task->task), 0, sizeof(task->task));
	memcpy(&(task->task.base), base, sizeof(t_task_base));
	task->task.user = NULL;

	ip2str(task->task.sub.peerip, ip);
	task->task.sub.oper_type = SYNC_2_GROUP;
	vfs_set_task(task, TASK_WAIT_SYNC);
	set_task_to_tmp(task);
}

int sync_task_2_group(t_vfs_tasklist *task)
{
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	t_cs_dir_info cs;
	t_cs_dir_info * csinfo = &cs;
	if (get_cs_info_by_path(task->task.base.filename, csinfo))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "get_cs_info_by_path %s ERROR!\n", task->task.base.filename);
		return -1;
	}
	task->task.base.offsize = 0;

	vfs_cs_peer *peer;
	int i = 0;
	for (i = 0; i < csinfo->index; i++)
	{
		if (self_ipinfo.archive)
		{
			if (csinfo->real_isp[i] != self_ipinfo.real_isp || csinfo->archive_isp[i] != self_ipinfo.archive_isp)
				continue;
		}
		else if (csinfo->isp[i] != self_ipinfo.isp)
			continue;

		if (csinfo->ip[i] == str2ip(self_ipinfo.s_ip))
			continue;
		if (find_ip_stat(csinfo->ip[i], &peer))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip[%u] not in active !\n", csinfo->ip[i]);
			create_delay_task(csinfo->ip[i], &(task->task.base));
			continue;
		}
		if (get_ip_info_by_uint(&ipinfo, csinfo->ip[i], 1, " ", " "))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "dispatch get_ip_info %u err %m\n", csinfo->ip[i]);
			continue;
		}
		if (ipinfo->offline)
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "dispatch %u offline\n", csinfo->ip[i]);
			continue;
		}
		do_dispatch_task(csinfo->ip[i], task, peer);
	}
	return 0;
}

void do_sync_dir_req(int fd, t_vfs_sig_body *b)
{
	t_vfs_tasklist *task = NULL;
	char obuf[2048] = {0x0};
	int n = 0;
	if (vfs_get_task(&task, TASK_HOME))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "fd[%d] do_sync_dir_req ERROR!\n", fd);
		memset(obuf, 0, sizeof(obuf));
		n = create_sig_msg(SYNC_DIR_RSP, TASK_SYNC_DIR, b, obuf, sizeof(t_vfs_sync_task));
		set_client_data(fd, obuf, n);
		modify_fd_event(fd, EPOLLOUT);
		return;
	}
	task->task.user = NULL;
	t_task_base *base = &(task->task.base);
	memset(base, 0, sizeof(t_task_base));
	memcpy(base, b, sizeof(t_vfs_sync_task));
	base->dstip = getpeerip(fd);
	vfs_set_task(task, TASK_Q_SYNC_DIR_REQ);
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
		LOG(glogfd, LOG_NORMAL, "no more del file [%s] [%s]\n", task->domain, ctime(&(task->starttime)));
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

