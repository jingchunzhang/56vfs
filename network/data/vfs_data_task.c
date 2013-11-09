/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_data_task.h"
#include "vfs_data.h"
#include "vfs_task.h"
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
#include "acl.h"

extern int vfs_sig_log;
extern t_ip_info self_ipinfo;

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

int do_recv_task(int fd, t_vfs_sig_head *h, t_task_base *base)
{
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	if (peer->sock_stat != PREPARE_RECVFILE)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] status not recv [%x] file [%s]\n", fd, peer->sock_stat, base->filename);
		return RECV_CLOSE;
	}
	t_vfs_tasklist *task0 = peer->recvtask;
	t_task_base *base0 = &(task0->task.base);
	t_task_sub *sub0 = &(task0->task.sub);
	if (h->status != FILE_SYNC_DST_2_SRC)
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] status err file [%s][%x]\n", fd, base->filename, h->status);
		peer->sock_stat = IDLE;
		task0->task.base.overstatus = OVER_E_OPEN_SRCFILE;
		vfs_set_task(task0, TASK_FIN);
		peer->recvtask = NULL;
		return RECV_CLOSE;
	}

	if (strcmp(base0->filemd5, base->filemd5))
		memcpy(base0, base, sizeof(t_task_base));
	if (g_config.continue_flag == 0)
		base0->offsize = 0;
	if (base0->offsize > 0)
		LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] file [%s] recv continue [%ld:%ld]\n", fd, base->filename, base->fsize, base0->offsize);
	sub0->processlen = base->fsize - base0->offsize;
	if (peer->local_in_fd > 0)
		close(peer->local_in_fd);
	if (open_tmp_localfile_4_write(base0, &(peer->local_in_fd)) != LOCALFILE_OK) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "fd[%d] file [%s] open file err %m\n", fd, base->filename);
		if (peer->recvtask)
		{
			base0->overstatus = OVER_E_OPEN_DSTFILE;
			vfs_set_task(peer->recvtask, TASK_FIN);
			peer->recvtask = NULL;
		}
		return RECV_CLOSE;
	}
	else
	{
		peer->sock_stat = RECVFILEING;
		LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] file [%s][%s] prepare recv\n", fd, base->filename, base->filemd5);
		sub0->starttime = time(NULL);
	}
	return RECV_ADD_EPOLLIN;
}

void do_send_task(int fd, t_task_base *base, t_vfs_sig_head *h)
{
	struct conn *curcon = &acon[fd];
	curcon->send_len = 0;
	char obuf[2048] = {0x0};
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->hbtime = time(NULL);
	uint8_t tmp_status = FILE_SYNC_DST_2_SRC;

	if (base->fsize == 0 || g_config.data_calcu_md5 || base->retry)
	{
		if (get_localfile_stat(base) != LOCALFILE_OK)
		{
			LOG(vfs_sig_log, LOG_ERROR, "filename[%s] get_localfile_stat ERROR %m!\n", base->filename);
			tmp_status = FILE_SYNC_DST_2_SRC_E_OPENFILE;
		}
	}

	if (tmp_status == FILE_SYNC_DST_2_SRC)
	{
		t_vfs_tasklist *task = NULL;
		if (vfs_get_task(&task, TASK_HOME))
		{
			LOG(vfs_sig_log, LOG_ERROR, "filename[%s] do_newtask ERROR!\n", base->filename);
			tmp_status = FILE_SYNC_DST_2_SRC_E_MALLOC;
		}
		else
		{
			t_task_sub *sub = &(task->task.sub);
			memset(sub, 0, sizeof(t_task_sub));
			ip2str(sub->peerip, peer->ip);
			if (g_config.continue_flag == 0)
				base->offsize = 0;
			if (base->offsize > 0)
				LOG(vfs_sig_log, LOG_NORMAL, "fd[%d] file [%s] send continue [%ld:%ld]\n", fd, base->filename, base->fsize, base->offsize);
			sub->processlen = base->fsize - base->offsize;
			sub->starttime = time(NULL);
			sub->oper_type = OPER_GET_RSP;
			sub->need_sync = TASK_SRC_NOSYNC;
			memcpy(&(task->task.base), base, sizeof(t_task_base));

			int lfd = -1;
			if (open_localfile_4_read(base, &lfd) != LOCALFILE_OK)
			{
				LOG(vfs_sig_log, LOG_ERROR, "fd[%d] err open [%s] %m close it\n", fd, base->filename);
				tmp_status = FILE_SYNC_DST_2_SRC_E_OPENFILE;
				task->task.base.overstatus = OVER_E_OPEN_SRCFILE;
				sub->oper_type = OPER_GET_REQ;
				vfs_set_task(task, TASK_FIN);
			}
			else
			{
				task->task.base.overstatus = OVER_UNKNOWN;
				vfs_set_task(task, TASK_SEND);
				peer->sendtask = task;
				set_client_fd(fd, lfd, base->offsize, sub->processlen);
				peer->sock_stat = SENDFILEING;
			}
		}
	}
	int n = create_sig_msg(CMD_GET_FILE_RSP, tmp_status, (t_vfs_sig_body *)base, obuf, sizeof(t_task_base));
	set_client_data(fd, obuf, n);
	peer->headlen = n;
}

