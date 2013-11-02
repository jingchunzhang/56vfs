/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *base文件，查询基础配置信息，设置相关基础状态及其它```
 *Tracker 数目较少，放在一个静态数组
 *CS和FCS数目较多，放在hash链表
 *CS FCS ip信息采用uint32_t 存储，便于存储和查找
 */
#include "c_api.h"
volatile extern int maintain ;		//1-维护配置 0 -可以使用

static void active_connect(t_ip_info *ipinfo)
{
	char *t = ipinfo->s_ip;
	int port = g_config.sig_port;
	int fd = createsocket(t, port);
	if (fd < 0)
	{
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "%s connect %s:%d err %m\n", self_ipinfo.sip, t, port);
		SetStr(VFS_STR_CONNECT_E, val);
		LOG(vfs_sig_log_err, LOG_ERROR, "connect %s:%d err %m\n", t, port);
		return;
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return;
	}
	add_fd_2_efd(fd);
	struct conn *curcon = &acon[fd];
	vfs_tracker_peer *peer = (vfs_tracker_peer *) curcon->user;
	peer->archive_isp = ipinfo->archive_isp;
	LOG(vfs_sig_log, LOG_NORMAL, "connect %s:%d fd:%d:%d\n", t, port, fd, peer->fd);
	t_vfs_sig_head h;
	t_vfs_sig_body b;
	h.bodylen = strlen(self_ipinfo.sip);
	memcpy(b.body, self_ipinfo.sip, h.bodylen);
	h.cmdid = ADDONE_REQ;
	if (ipinfo->role == ROLE_TRACKER)
		h.status = T_A_2_T;
	else
		h.status = T_A_2_F;
	peer->role = ipinfo->role;
	active_send(peer, &h, &b);
}

/*find 活动链接信息 */
int find_ip_stat(uint32_t ip, vfs_tracker_peer **dpeer)
{
	list_head_t *hashlist = &(cfg_list[ALLMASK&ip]);
	vfs_tracker_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, hashlist, hlist)
	{
		if (peer->cfgip == ip)
		{
			*dpeer = peer;
			return 0;
		}
	}

	hashlist = &(online_list[ALLMASK&ip]);
	peer = NULL;
	list_for_each_entry_safe_l(peer, l, hashlist, hlist)
	{
		if (peer->ip == ip)
		{
			*dpeer = peer;
			return 0;
		}
	}
	return -1;
}

static void scan_cfg_iplist_and_connect()
{
	list_head_t *hashlist; 
	t_ip_info_list *ipinfo = NULL;
	list_head_t *l;
	vfs_tracker_peer *peer = NULL;
	static int i = 0;
	int once = 0;
	if (get_cfg_lock())
		return ;
	for (; i < 256; i++)
	{
		hashlist = &(cfg_iplist[i]);
		list_for_each_entry_safe_l(ipinfo, l, hashlist, hlist)
		{
			if (ipinfo->ipinfo.isself)
				continue;
			if (ipinfo->ipinfo.role != ROLE_FCS)
				continue;
			if (find_ip_stat(ipinfo->ipinfo.ip, &peer))
				active_connect(&(ipinfo->ipinfo));
		}
		once++;
		if (once >= 10)
			break;
	}
	if (i >= 256)
		i = 0;
	release_cfg_lock();
}
