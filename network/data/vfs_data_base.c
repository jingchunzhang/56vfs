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
volatile extern int maintain ;		//1-维护配置 0 -可以使用
extern t_vfs_up_proxy g_proxy;

static int get_ip_connect_count(uint32_t ip)
{
	int count = 0;
	list_head_t *hashlist = &(online_list[ALLMASK&ip]);
	vfs_cs_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, hashlist, hlist)
	{
		if (peer->ip == ip && peer->mode == CON_ACTIVE)
		{
			count++;
		}
	}
	return count;
}

static inline int isDigit(const char *ptr) 
{
	return isdigit(*(unsigned char *)ptr);
}

static int establish_proxy_connection(int fd, char *host, int port, char *proxy_user, char *proxy_pass)
{
	char *cp, buffer[1024] = {0x0};
	char *authhdr, authbuf[1024] = {0x0};
	int len;

	if (strlen(proxy_user) && strlen(proxy_pass)) {
		LOG(vfs_sig_log, LOG_NORMAL, "authentication information \n");
		snprintf(buffer, sizeof(buffer), "%s:%s", proxy_user, proxy_pass);
		len = strlen(buffer);

		if ((len*8 + 5) / 6 >= (int)sizeof authbuf - 3) {
			LOG(vfs_sig_log, LOG_ERROR, "authentication information is too long\n");
			return -1;
		}

		base64_encode(buffer, len, authbuf, 1);
		authhdr = "\r\nProxy-Authorization: Basic ";
	} else {
		*authbuf = '\0';
		authhdr = "";
	}

	snprintf(buffer, sizeof buffer, "CONNECT %s:%d HTTP/1.1%s%s\r\n\r\n",
		 host, port, authhdr, authbuf);
	len = strlen(buffer);
	if (write(fd, buffer, len) != len) {
		LOG(vfs_sig_log, LOG_ERROR, "failed to write to proxy");
		return -1;
	}

	for (cp = buffer; cp < &buffer[sizeof buffer - 1]; cp++) {
		if (read(fd, cp, 1) != 1) {
			LOG(vfs_sig_log, LOG_ERROR, "failed to read from proxy");
			return -1;
		}
		if (*cp == '\n')
			break;
	}

	if (*cp != '\n')
		cp++;
	*cp-- = '\0';
	if (*cp == '\r')
		*cp = '\0';
	if (strncmp(buffer, "HTTP/", 5) != 0) {
		LOG(vfs_sig_log, LOG_ERROR, "bad response from proxy -- %s\n", buffer);
		return -1;
	}
	for (cp = &buffer[5]; isDigit(cp) || *cp == '.'; cp++) {}
	while (*cp == ' ')
		cp++;
	if (*cp != '2') {
		LOG(vfs_sig_log, LOG_ERROR, "bad response from proxy -- %s\n", buffer);
		return -1;
	}
	/* throw away the rest of the HTTP header */
	while (1) {
		for (cp = buffer; cp < &buffer[sizeof buffer - 1]; cp++) {
			if (read(fd, cp, 1) != 1) {
				LOG(vfs_sig_log, LOG_ERROR, "failed to read from proxy %m");
				return -1;
			}
			if (*cp == '\n')
				break;
		}
		if (cp > buffer && *cp == '\n')
			cp--;
		if (cp == buffer && (*cp == '\n' || *cp == '\r'))
			break;
	}
	return 0;
}

static void active_connect(t_ip_info *ipinfo)
{
	char *t = ipinfo->s_ip;
	int port = g_config.data_port;
	if (g_proxyed)
	{
		t = g_proxy.host;
		port = g_proxy.port;
	}
	int fd = createsocket(t, port);
	if (fd < 0)
	{
		char val[256] = {0x0};
		snprintf(val, sizeof(val), "%s connect %s:%d err %m\n", self_ipinfo.sip, t, port);
		SetStr(VFS_STR_CONNECT_E, val);
		LOG(vfs_sig_log, LOG_ERROR, "connect %s:%d err %m\n", t, port);
		return;
	}
	if (g_proxyed)
	{
		if (establish_proxy_connection(fd, ipinfo->s_ip, g_config.data_port, g_proxy.username, g_proxy.password))
		{
			LOG(vfs_sig_log, LOG_ERROR, "establish_proxy_connection err %m!\n");
			close(fd);
			return;
		}
	}
	if (svc_initconn(fd))
	{
		LOG(vfs_sig_log, LOG_ERROR, "svc_initconn err %m\n");
		close(fd);
		return;
	}
	add_fd_2_efd(fd);
	LOG(vfs_sig_log, LOG_NORMAL, "connect %s:%d\n", t, port);
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->sock_stat = IDLE;
	peer->mode = CON_ACTIVE;
}

/*find 活动链接信息 */
int find_ip_stat(uint32_t ip, vfs_cs_peer **dpeer, int mode, int status)
{
	list_head_t *hashlist = &(online_list[ALLMASK&ip]);
	vfs_cs_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, hashlist, hlist)
	{
		if (peer->ip == ip)
		{
			if (mode == peer->mode && status == peer->sock_stat)
			{
				*dpeer = peer;
				return 0;
			}
		}
	}
	return -1;
}

void check_task()
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_WAIT_TMP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(task, TASK_WAIT);
	}

	while (1)
	{
		ret = vfs_get_task(&task, TASK_Q_SYNC_DIR_TMP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(task, TASK_Q_SYNC_DIR);
	}

	uint16_t once_run = 0;
	while (1)
	{
		once_run++;
		if (once_run >= g_config.cs_max_task_run_once)
			return;
		ret = vfs_get_task(&task, TASK_WAIT);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			ret = vfs_get_task(&task, TASK_Q_SYNC_DIR);
			if (ret != GET_TASK_OK)
				return ;
			else
				LOG(vfs_sig_log, LOG_DEBUG, "Process TASK_Q_SYNC_DIR!\n");
		}
		t_task_sub *sub = &(task->task.sub);
		t_task_base *base = &(task->task.base);
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s be get from wait queue!\n", base->filename, base->src_domain);
		if (base->retry > g_config.retry)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s too many try!\n", base->filename, base->src_domain, base->retry, g_config.retry);
			real_rm_file(base->tmpfile);
			base->overstatus = OVER_TOO_MANY_TRY;
			vfs_set_task(task, TASK_FIN);  
			continue;
		}
		if (check_localfile_md5(base, VIDEOFILE) == LOCALFILE_OK)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s check md5 ok!\n", base->filename, base->src_domain);
			base->overstatus = OVER_OK;
			vfs_set_task(task, TASK_FIN);  
			continue;
		}
		t_ip_info ipinfo0;
		t_ip_info *ipinfo = &ipinfo0;
		if(get_ip_info(&ipinfo, sub->peerip, 1))
		{
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s:%s be hung up because get_ip_info error!\n", base->filename, base->src_domain, sub->peerip);
			base->overstatus = OVER_E_IP;
			vfs_set_task(task, TASK_FIN);
			continue;
		}
		vfs_cs_peer *peer = NULL;
		find_ip_stat(str2ip(sub->peerip), &peer, CON_ACTIVE, IDLE);
		if (sub->oper_type != OPER_GET_REQ)
		{
			LOG(vfs_sig_log, LOG_ERROR, "%s:%d ERROR oper_type %d %s!\n", ID, LN, sub->oper_type, base->filename);
			base->overstatus = OVER_E_TYPE;
			vfs_set_task(task, TASK_FIN);
			continue;
		}
		LOG(vfs_sig_log, LOG_DEBUG, "%s:%s is prepare OPER_GET_REQ from %s\n", base->filename, base->src_domain, sub->peerip);
		if(check_disk_space(base) != DISK_OK)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%d filename[%s] DISK NOT ENOUGH SPACE!\n", ID, LN, base->filename);
			if (DISK_SPACE_TOO_SMALL == check_disk_space(base))
			{
				if (sub->need_sync == TASK_SYNC_ISDIR)
					vfs_set_task(task, TASK_Q_SYNC_DIR_TMP);
				else
					vfs_set_task(task, TASK_WAIT_TMP);
			}
			else
			{
				base->overstatus = OVER_E_OPEN_DSTFILE;
				vfs_set_task(task, TASK_FIN);
			}
			continue;
		}

		if (peer == NULL)
		{
			int count = get_ip_connect_count(str2ip(sub->peerip));
			if (ipinfo->role == ROLE_FCS && count > g_config.fcs_max_connects)
			{
				LOG(vfs_sig_log, LOG_DEBUG, "ip %s too many connect %d max %d\n", sub->peerip, count, g_config.fcs_max_connects);
				if (sub->need_sync == TASK_SYNC_ISDIR)
					vfs_set_task(task, TASK_Q_SYNC_DIR_TMP);
				else
					vfs_set_task(task, TASK_WAIT_TMP);
				continue;
			}
			if (ipinfo->role == ROLE_CS && count > g_config.cs_max_connects)
			{
				LOG(vfs_sig_log, LOG_DEBUG, "ip %s too many connect %d max %d\n", sub->peerip, count, g_config.cs_max_connects);
				if (sub->need_sync == TASK_SYNC_ISDIR)
					vfs_set_task(task, TASK_Q_SYNC_DIR_TMP);
				else
					vfs_set_task(task, TASK_WAIT_TMP);
				continue;
			}
			active_connect(ipinfo);
			char *peerhost = sub->peerip;
			if (g_proxyed)
				peerhost = g_proxy.host;

			if (find_ip_stat(str2ip(peerhost), &peer, CON_ACTIVE, IDLE) != 0)
			{
				LOG(vfs_sig_log, LOG_DEBUG, "%s:%s be hung up because find_ip_stat error!\n", base->filename, base->src_domain);
				if (sub->need_sync == TASK_SYNC_ISDIR)
					vfs_set_task(task, TASK_Q_SYNC_DIR_TMP);
				else
					vfs_set_task(task, TASK_WAIT_TMP);
				continue;
			}
		}
		if (g_config.vfs_test)
		{
			LOG(vfs_sig_log, LOG_NORMAL, "vfs run in test %s %s\n", base->filename, base->src_domain);
			base->overstatus = OVER_OK;
			vfs_set_task(task, TASK_FIN);
			continue;
		}
		t_vfs_sig_head h;
		t_vfs_sig_body b;
		sub->lastlen = 0;
		h.bodylen = sizeof(t_task_base);
		memcpy(b.body, base, sizeof(t_task_base));
		h.cmdid = CMD_GET_FILE_REQ;
		h.status = FILE_SYNC_DST_2_SRC;
		active_send(peer, &h, &b);
		peer->sock_stat = PREPARE_RECVFILE;
		LOG(vfs_sig_log, LOG_DEBUG, "fd[%d:%s] %s:%s send get a file sock_stat [%s]!\n", peer->fd, sub->peerip, base->filename, base->src_domain, sock_stat_cmd[peer->sock_stat]);
		vfs_set_task(task, TASK_RECV);
		peer->recvtask = task;
	}
}


