/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/


#define SYNCFILESTR "vfs_cmd=M_SYNCFILE"
#define SYNCFILESTRLEN 18

static int init_cfg_connect(char *sip, vfs_voss_peer *peer)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	if (ip == INADDR_NONE)
	{
		LOG(vfs_voss_log, LOG_ERROR, "err ip %s %u\n", sip, peer->ip);
		return -1;
	}
	t_ip_info ipinfo0;
	t_ip_info *ipinfo = &ipinfo0;
	if (get_ip_info(&ipinfo, sip, 1))
	{
		LOG(vfs_voss_log, LOG_ERROR, "get_ip_info err ip %s %u %u\n", sip, ip, peer->ip);
		peer->role = ROLE_CS;
	}
	else
		peer->role = ipinfo->role;
	peer->cfgip = ip;
	peer->sock_stat = LOGIN;
	list_add_head(&(peer->cfglist), &cfg_list[ip&ALLMASK]);
	return 0;
}

static int check_close_local(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	LOG(vfs_voss_log, LOG_TRACE, "fd[%d] sock stat %s!\n", fd, sock_stat_cmd[peer->sock_stat]);
	time_t now = time(NULL);
	t_voss_data_info *datainfo = &(peer->datainfo);
	if (peer->local_in_fd > 0 && (now - datainfo->opentime >= g_config.voss_interval))
	{
		close(peer->local_in_fd);
		peer->local_in_fd = -1;
		char workfile[256] = {0x0};
		snprintf(workfile, sizeof(workfile), "%s", datainfo->outfile);
		
		//link content to sync dir, for another cdc 
		char syncfile[256] = {0x0};
		snprintf(syncfile, sizeof(syncfile), "%s/%s/%s", g_config.path, path_dirs[PATH_SYNCDIR], basename(datainfo->outfile));
		if (link(workfile, syncfile))
			LOG(vfs_voss_log, LOG_ERROR, "fd[%d] link [%s][%s] err %m\n", fd, workfile, syncfile);

		char outfile[256] = {0x0};
		snprintf(outfile, sizeof(outfile), "%s/%s/%s", g_config.path, path_dirs[PATH_OUTDIR], basename(datainfo->outfile));
		if (rename(workfile, outfile))
			LOG(vfs_voss_log, LOG_ERROR, "fd[%d] rename [%s][%s] err %m\n", fd, workfile, outfile);


	}
	return RECV_ADD_EPOLLOUT;  
}

static int sub_recv(int fd)
{
	struct conn *curcon = &acon[fd];
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	peer->hbtime = time(NULL);
	list_move_tail(&(peer->alist), &activelist);
	LOG(vfs_voss_log, LOG_TRACE, "fd[%d] sock stat %s!\n", fd, sock_stat_cmd[peer->sock_stat]);
	if (peer->sock_stat == PREPARE_SYNCFILE || peer->sock_stat == SYNCFILEING)
	{
		LOG(vfs_voss_log, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
		char *data;
		size_t datalen;
		if (get_client_data(fd, &data, &datalen))
		{
			LOG(vfs_voss_log, LOG_TRACE, "%s:%d fd[%d] no data!\n", FUNC, LN, fd);
			return RECV_ADD_EPOLLIN;  
		}
		t_voss_data_info *datainfo = &(peer->datainfo);
		int remainlen = datainfo->datalen - datainfo->recvlen;
		datalen = datalen <= remainlen ? datalen : remainlen ; 
		int n = write(peer->local_in_fd, data, datalen);
		if (n < 0)
		{
			LOG(vfs_voss_log, LOG_ERROR, "fd[%d] write error %m close it!\n", fd);
			return RECV_CLOSE;  /* ERROR , close it */
		}
		consume_client_data(fd, n);
		datainfo->recvlen += n;

		if (datainfo->recvlen >= datainfo->datalen)
		{
			peer->sock_stat = IDLE;
			return check_close_local(fd);
		}
		else
			return RECV_ADD_EPOLLIN;  /*no suffic data, need to get data more */
	}
	return RECV_ADD_EPOLLIN;  
}

static int process_data_req(int fd, t_head_info *h)
{
	struct conn *curcon = &acon[fd];
	vfs_voss_peer *peer = (vfs_voss_peer *) curcon->user;
	LOG(vfs_voss_log, LOG_DEBUG, "ip[%s] [%s]\n", peer->ip, FUNC);
	if (peer->local_in_fd < 0)
	{
		char day[16] = {0x0};
		get_strtime(day);
		char *filename = peer->datainfo.outfile;
		memset(filename, 0, sizeof(peer->datainfo.outfile));
		snprintf(filename, sizeof(peer->datainfo.outfile), "%s/%s/%s_%s%s_%s%s", g_config.path, path_dirs[PATH_WKDIR], iprole[peer->role], VOSSPREFIX, peer->ip, day, VOSSSUFFIX);
		int localfd = open(filename, O_CREAT | O_RDWR | O_APPEND | O_LARGEFILE, 0644);
		if (localfd < 0)
		{
			LOG(vfs_voss_log, LOG_ERROR, "open %s err %m\n", filename);
			return RECV_CLOSE;
		}
		peer->local_in_fd = localfd;
		t_voss_data_info *datainfo = &(peer->datainfo);
		datainfo->opentime = time(NULL);
		LOG(vfs_voss_log, LOG_DEBUG, "ip[%s] open [%s]\n", peer->ip, filename);
	}
	peer->sock_stat = SYNCFILEING;
	peer->datainfo.datalen = h->totallen - HEADSIZE;
	peer->datainfo.recvlen = 0;
	consume_client_data(fd, HEADSIZE);
	return sub_recv(fd);
}

int find_ip_stat(uint32_t ip, vfs_voss_peer **dpeer)
{
	list_head_t *hashlist = &(cfg_list[ALLMASK&ip]);
	vfs_voss_peer *peer = NULL;
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
		if (peer->con_ip == ip)
		{
			*dpeer = peer;
			return 0;
		}
	}
	return -1;
}

static void do_peer_msg(char *msg, vfs_voss_peer *peer)
{
	char *p = strrchr(msg, '\n');
	if (p)
		*p = 0x0;
	LOG(vfs_voss_log, LOG_NORMAL, "ip %s msg [%s]\n", peer->ip, msg);
	t_head_info head;
	memset(&head, 0, sizeof(head));
	if (strncasecmp(msg, "vfs_cmd=stopvfs", strlen("vfs_cmd=stopvfs")) == 0)
	{
		LOG(vfs_voss_log, LOG_NORMAL, "ip %s msg [%s] send REQ_STOPVFS\n", peer->ip, msg);
		create_voss_head((char *)&head, REQ_STOPVFS, 0);
		set_client_data(peer->fd, (char *)&head, HEADSIZE);
		modify_fd_event(peer->fd, EPOLLOUT);
		return;
	}
	int sendlen = strlen(msg);
	create_voss_head((char *)&head, REQ_VFS_CMD, sendlen);
	set_client_data(peer->fd, (char *)&head, HEADSIZE);
	set_client_data(peer->fd, msg, sendlen);
	modify_fd_event(peer->fd, EPOLLOUT);
}

static void do_ip_msg(char *msg, char *sip)
{
	char s_ip[16] = {0x0};
	uint32_t ip = get_uint32_ip(sip, s_ip);
	if (ip == INADDR_NONE)
	{
		LOG(vfs_voss_log, LOG_ERROR, "err ip %s %s\n", sip, msg);
		return ;
	}

	vfs_voss_peer *peer;
	if (find_ip_stat(ip, &peer))
	{
		LOG(vfs_voss_log, LOG_ERROR, "err ip %s %s not online\n", sip, msg);
		return;
	}
	return do_peer_msg(msg, peer);
}

static void do_group_msg(char *msg, int group)
{
	vfs_voss_peer *peer = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(peer, l, &activelist, alist)
	{
		if (group == 0)
			do_peer_msg(msg, peer);
		else if (group == peer->role)
			do_peer_msg(msg, peer);
	}
}

static void do_sub_msg(char *buf)
{
	if (strncasecmp(buf, "iplist=", 7))
	{
		LOG(vfs_voss_log, LOG_ERROR, "no iplist= in %s\n", buf);
		return;
	}
	char *s = buf + 7;
	char *e = strchr(buf, '&');
	if (e == NULL)
	{
		LOG(vfs_voss_log, LOG_ERROR, "no & in %s\n", buf);
		return;
	}
	*e = 0x0;
	if (strncasecmp(s, "allip", 5) == 0)
		return do_group_msg(e + 1, UNKOWN);
	if (strncasecmp(s, "allcs", 5) == 0)
		return do_group_msg(e + 1, ROLE_CS);
	if (strncasecmp(s, "alltracker", 10) == 0)
		return do_group_msg(e + 1, ROLE_TRACKER);
	if (strncasecmp(s, "allfcs", 6) == 0)
		return do_group_msg(e + 1, ROLE_FCS);

	while (1)
	{
		char *t = strchr(s, ',');
		if (t == NULL)
			break;
		*t = 0x0;
		do_ip_msg(e + 1, s);
		s = t + 1;
	}
	do_ip_msg(e + 1, s);
}

static void check_indir()
{
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s/%s", g_config.path, path_dirs[PATH_INDIR]);
	char obuf[20480] = {0x0};
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(path)) == NULL) 
		LOG(vfs_voss_log, LOG_ERROR, "opendir [%s] %s err  %m\n", g_config.path, path);
	else
	{
		while((dirp = readdir(dp)) != NULL) 
		{
			if (dirp->d_name[0] == '.')
				continue;
			char file[256] = {0x0};
			snprintf(file, sizeof(file), "%s/%s", path, dirp->d_name);

			FILE *fp = fopen(file, "r");
			if (fp == NULL)
			{
				LOG(vfs_voss_log, LOG_ERROR, "openfile %s err  %m\n", file);
				continue;
			}
			while (fgets(obuf, sizeof(obuf), fp))
			{
				do_sub_msg(obuf);
				memset(obuf, 0, sizeof(obuf));
			}
			fclose(fp);

			char bkfile[256] = {0x0};
			snprintf(bkfile, sizeof(bkfile), "%s/%s/%s", g_config.path, path_dirs[PATH_BKDIR], dirp->d_name);
			if (rename(file, bkfile))
			{
				LOG(vfs_voss_log, LOG_ERROR, "rename [%s] [%s] err %m\n", file, bkfile);
				unlink(file);
			}
		}
		closedir(dp);
	}
}
