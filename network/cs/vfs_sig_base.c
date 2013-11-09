/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 *CS FCS ip信息采用uint32_t 存储，便于存储和查找
 */
#include "vfs_file_filter.h"
volatile extern int maintain ;		//1-维护配置 0 -可以使用

static void do_sync_dir_req_sub(char *o, t_task_base *sbase)
{
	sbase->fsize = 0;
	t_vfs_sync_task *task = (t_vfs_sync_task *) sbase;

	char *datadir = myconfig_get_value("vfs_cs_datadir");
	if (datadir == NULL)
		datadir = "/diska";
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s/%d/%d/%s/flvdownload", datadir, task->d1, task->d2, task->domain);
	char realpath[256] = {0x0};
	snprintf(realpath, sizeof(realpath), "%s/%d/%d", datadir, task->d1, task->d2);

	char obuf[2048] = {0x0};
	int n = 0;
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(path)) == NULL) 
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "opendir %s err  %m\n", path);
		sbase->overstatus = OVER_E_OPEN_SRCFILE;
	}
	else
	{
		while((dirp = readdir(dp)) != NULL) 
		{
			if (dirp->d_name[0] == '.')
				continue;

			if (self_ipinfo.isp != MP4 && check_file_filter(dirp->d_name))
			{
				LOG(vfs_sig_log, LOG_TRACE, "syncdir req filename %s check not ok!\n", dirp->d_name);
				continue;
			}

			//mp4 cs only sync mp4 file
			if (self_ipinfo.isp == MP4 && check_mp4_filter(dirp->d_name))
			{
				LOG(vfs_sig_log, LOG_TRACE, "mp4 cs only sync mp4 file, syncdir req filename %s check not ok!\n", dirp->d_name);
				continue;
			}

			char file[256] = {0x0};
			snprintf(file, sizeof(file), "%s/%s", path, dirp->d_name);

			struct stat filestat;
			if(stat(file, &filestat) < 0) 
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "stat error,filename:%s\n", file);
				continue;
			}
			if (!S_ISREG(filestat.st_mode))
				continue;
			if (task->starttime && filestat.st_ctime < task->starttime)
				continue;
			if (task->endtime && filestat.st_ctime > task->endtime)
				continue;
			if (access(file, R_OK))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "file access error,filename:%s:%m\n", file);
				continue;
			}

			t_task_base base;
			memset(&base, 0, sizeof(base));
			snprintf(base.filename, sizeof(base.filename), "%s/%s", realpath, dirp->d_name);
			snprintf(base.src_domain, sizeof(base.src_domain), "%s", task->domain);

			base.fsize = filestat.st_size;
			base.fmode = filestat.st_mode;
			base.mtime = filestat.st_mtime;
			base.ctime = filestat.st_ctime - g_config.task_timeout;

			unsigned char md5[33] = {0x0};
			if (getfilemd5view(file, md5))
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "md5 error,filename:%s %m\n", file);
				continue;
			}
			snprintf(base.filemd5, sizeof(base.filemd5), "%s", md5);
			base.type = TASK_ADDFILE;

			memset(obuf, 0, sizeof(obuf));
			n = create_sig_msg(NEWTASK_REQ, TASK_SYNC_DIR, (t_vfs_sig_body *)&base, obuf, sizeof(t_task_base));
			if (n + sbase->fsize >= MAXSYNCBUF)
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "too many file %s:%s\n", sbase->filename, sbase->src_domain);
				break;
			}
			o = mempcpy(o, obuf, n);
			sbase->fsize += n;
		}
		closedir(dp);
		sbase->overstatus = OVER_OK;
	}
	memset(obuf, 0, sizeof(obuf));
	n = create_sig_msg(SYNC_DIR_RSP, TASK_SYNC_DIR, (t_vfs_sig_body *)task, obuf, sizeof(t_vfs_sync_task));
	o = mempcpy(o, obuf, n);
	sbase->fsize += n;
}

static void active_connect(t_ip_info *ipinfo)
{
	char *t = ipinfo->s_ip;
	t_vfs_sig_head h;
	int port = g_config.sig_port;
	h.status = C_A_2_T;
	if (ipinfo->role == ROLE_CS)
		h.status = self_stat;
	if (ipinfo->role == ROLE_FCS)
		h.status = C_A_2_F;

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
	LOG(vfs_sig_log, LOG_NORMAL, "connect %s:%d role %d\n", t, port, ipinfo->role);
	struct conn *curcon = &acon[fd];
	vfs_cs_peer *peer = (vfs_cs_peer *) curcon->user;
	peer->role = ipinfo->role;
	t_vfs_sig_body b;
	h.bodylen = strlen(self_ipinfo.sip);
	memcpy(b.body, self_ipinfo.sip, h.bodylen);
	h.cmdid = ADDONE_REQ;
	active_send(peer, &h, &b);
}

/*find 活动链接信息 */
int find_ip_stat(uint32_t ip, vfs_cs_peer **dpeer)
{
	list_head_t *hashlist = &(cfg_list[ALLMASK&ip]);
	vfs_cs_peer *peer = NULL;
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
	if (self_ipinfo.isp >= EDU && self_ipinfo.isp != MP4)
	{
		LOG(vfs_sig_log, LOG_NORMAL, "little isp , not need connect other but voss!\n");
		return;
	}

	list_head_t *hashlist; 
	t_ip_info_list *ipinfo = NULL;
	list_head_t *l;
	vfs_cs_peer *peer = NULL;
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
			if (ipinfo->ipinfo.role == ROLE_CS && self_ipinfo.archive_isp == ISP_FCS)
			{
				if (self_ipinfo.isp == MP4)
				{
					if (ipinfo->ipinfo.role != ROLE_FCS)
						continue;
				}
				if (ipinfo->ipinfo.role == ROLE_CS && ipinfo->ipinfo.isp >= EDU)
					continue;
			}
			if (ipinfo->ipinfo.role == ROLE_CS && strcmp(self_ipinfo.dirs[0], ipinfo->ipinfo.dirs[0]))
				continue;
			if (ipinfo->ipinfo.role == ROLE_VOSS_MASTER)
				continue;
			if (find_ip_stat(ipinfo->ipinfo.ip, &peer))
			{
				if (ipinfo->ipinfo.offline)
					continue;
				if (self_stat != OFF_LINE)
					active_connect(&(ipinfo->ipinfo));
			}
			else
			{
				if (self_stat == OFF_LINE)
					do_close(peer->fd);
			}
		}
		once++;
		if (once >= 20)
			break;
	}
	release_cfg_lock();
	if (i >= 256)
		i = 0;
}

static void do_sub_sync(t_vfs_sync_list *vfs_sync, vfs_cs_peer *peer)
{
	char sip[16] = {0x0};
	ip2str(sip, peer->ip);
	int d1 = vfs_sync->sync_task.d1;
	int d2 = vfs_sync->sync_task.d2;
	LOG(vfs_sig_log, LOG_NORMAL, "%s:%s:%d  sync [%d:%d] to [%s]!\n", ID, FUNC, LN, d1, d2, sip);
	char obuf[2048] = {0x0};
	int n = 0;
	if (vfs_sync->sync_task.type == TASK_ADDFILE)
		n = create_sig_msg(SYNC_DIR_REQ, TASK_SYNC, (t_vfs_sig_body *)&(vfs_sync->sync_task), obuf, sizeof(vfs_sync->sync_task));
	else
		n = create_sig_msg(SYNC_DEL_REQ, TASK_SYNC, (t_vfs_sig_body *)&(vfs_sync->sync_task), obuf, sizeof(vfs_sync->sync_task));
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	peer->vfs_sync_list = vfs_sync;
	sync_para.flag = 1;
	sync_para.last = time(NULL);
}

static int decompose_sync_dir_archive(t_vfs_sync_task *sync_task)
{
	int d1 = sync_task->d1;
	int d2 = sync_task->d2;

	t_cs_dir_info cs;
	t_cs_dir_info * csinfo = &cs;
	if (get_cs_info(d1, d2, csinfo))
	{
		LOG(vfs_sig_log_err, LOG_ERROR, "get_cs_info err %d %d!\n", d1, d2);
		return -1;
	}
	uint32_t selfip = str2ip(self_ipinfo.sip);
	int j = 0;
	for (j = 0; j < csinfo->index; j++)
	{
		if (csinfo->archive_isp[j] == ISP_FCS && csinfo->isp[j] != self_ipinfo.real_isp)
			continue;
		if (csinfo->archive_isp[j] != ISP_FCS && csinfo->archive_isp[j] != self_ipinfo.archive_isp)
			continue;
		if (csinfo->ip[j] == selfip)
			continue;
		t_vfs_sync_list *vfs_sync = (t_vfs_sync_list *)malloc(sizeof(t_vfs_sync_list));
		if (vfs_sync == NULL)
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "malloc err, abort! %m\n");
			stop = 1;
			pthread_exit(NULL);
		}
		memset(vfs_sync, 0, sizeof(t_vfs_sync_list));
		memcpy(&(vfs_sync->sync_task), sync_task, sizeof(t_vfs_sync_task));
		vfs_sync->sync_task.ip = csinfo->ip[j];
		INIT_LIST_HEAD(&(vfs_sync->list));
		list_add_tail(&(vfs_sync->list), &sync_list);
	}
	return 0;
}

static void do_active_sync_archive()
{
	t_vfs_sync_list *vfs_sync = NULL;
	list_head_t tmp_list;
	INIT_LIST_HEAD(&tmp_list);
	list_head_t *l;
	list_for_each_entry_safe_l(vfs_sync, l, &sync_list, list)
	{
		list_del_init(&(vfs_sync->list));
		t_vfs_sync_task *sync_task = (t_vfs_sync_task *)&(vfs_sync->sync_task);
		if (sync_task->ip == 0)
		{
			if (decompose_sync_dir_archive(sync_task))
				list_add_tail(&(vfs_sync->list), &sync_list);
			else
				free(vfs_sync);
			continue;
		}
		vfs_cs_peer *peer = NULL;
		if(find_ip_stat(sync_task->ip, &peer))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip [%u] not on line !\n", sync_task->ip);
			list_add_tail(&(vfs_sync->list), &tmp_list);
			continue;
		}

		if (peer->vfs_sync_list)
			list_add_tail(&(vfs_sync->list), &tmp_list);
		else
			do_sub_sync(vfs_sync, peer);
	}

	list_for_each_entry_safe_l(vfs_sync, l, &tmp_list, list)
	{
		list_del_init(&(vfs_sync->list));
		list_add_tail(&(vfs_sync->list), &sync_list);
	}
}

static void do_active_sync()
{
	int run_sync_dir_count = get_task_count(TASK_Q_SYNC_DIR) + get_task_count(TASK_Q_SYNC_DIR_TMP);
	if (run_sync_dir_count >= 1024)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "so many sync_dir_task %d\n", run_sync_dir_count);
		return;
	}
	if (self_ipinfo.archive)
		return do_active_sync_archive();
	char curtime[16] = {0x0};
	get_strtime(curtime);
	char hour[12] = {0x0};
	snprintf(hour, sizeof(hour), "%.2s:%.2s:%.2s", curtime+8, curtime+10, curtime+12);
	if (strcmp(hour, g_config.sync_stime) < 0 || strcmp(hour, g_config.sync_etime) > 0)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "cur time %s not between %s %s, can not sync dir\n", hour, g_config.sync_stime, g_config.sync_etime);
		return;
	}
	t_vfs_sync_list *vfs_sync = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(vfs_sync, l, &sync_list, list)
	{
		list_del_init(&(vfs_sync->list));
		get = 1;
		break;
	}
	if (get == 0)
	{
		LOG(vfs_sig_log, LOG_NORMAL, "no sync_task in list!\n");
		sync_para.flag = 2;
		return;
	}
	int d1 = vfs_sync->sync_task.d1;
	int d2 = vfs_sync->sync_task.d2;
	LOG(vfs_sig_log, LOG_NORMAL, "get sync task %d %d %s\n", d1, d2, vfs_sync->sync_task.domain);

	int is_first_ip = 1;
	int flag = 0;
	uint32_t spec_ip = 0;
	t_cs_dir_info cs;
	t_cs_dir_info * csinfo = &cs;
	if (get_cs_info(d1, d2, csinfo))
	{
		LOG(vfs_sig_log, LOG_DEBUG, "next loop!\n");
		list_add_tail(&(vfs_sync->list), &sync_list);
		return;
	}
	vfs_cs_peer *peer = NULL;
	uint32_t selfip = str2ip(self_ipinfo.sip);
	int j = 0;
	for (j = 0; j < csinfo->index; j++)
	{
		if (csinfo->archive_isp[j] != UNKNOW_ISP)
			continue;
		if (csinfo->isp[j] != self_ipinfo.isp)
			continue;
		if (csinfo->ip[j] == selfip)
			continue;
		if (find_ip_stat(csinfo->ip[j], &peer))
		{
			LOG(vfs_sig_log, LOG_DEBUG, "ip[%u] not in active !\n", csinfo->ip[j]);
			continue;
		}
		if (csinfo->ip[j] < selfip)
			is_first_ip = 0;
		if (peer->server_stat == UNKOWN_STAT)
		{
			flag = 3;
			spec_ip = csinfo->ip[j];
			continue;
		}
		if (peer->server_stat == ON_LINE)
		{
			flag = 2;
			break;
		}
	}

	if (flag != 2)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "本组同运营商无ip 准备好，尝试同组跨运营商机器!\n");
		for (j = 0; j < csinfo->index; j++)
		{
			if (csinfo->archive_isp[j] != UNKNOW_ISP)
				continue;
			if (csinfo->isp[j] == self_ipinfo.isp)
				continue;
			if (csinfo->isp[j] > CNC)
				continue;
			if (csinfo->ip[j] == selfip)
				continue;
			if (find_ip_stat(csinfo->ip[j], &peer))
			{
				LOG(vfs_sig_log, LOG_DEBUG, "ip[%u] not in active !\n", csinfo->ip[j]);
				continue;
			}
			if (peer->server_stat == ON_LINE)
			{
				flag = 2;
				break;
			}
		}
	}

	if (flag == 3)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "ip[%u] stat is UNKOWN_STAT !\n", spec_ip);
		list_add_head(&(vfs_sync->list), &sync_list);
		return;
	}
	if (flag != 2)
	{
		if (is_first_ip == 0)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "all ip is wait for vfs_sync!\n");
			list_add_head(&(vfs_sync->list), &sync_list);
			return;
		}
		LOG(vfs_sig_log, LOG_DEBUG, "no cs ready for vfs_sync, but i am only or first_ip!\n");
		peer = NULL;
		t_ip_info ipinfo0;
		t_ip_info *ipinfo = &ipinfo0;
		if (get_ip_info(&ipinfo, vfs_sync->sync_task.domain, 1)) 
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip %s not in cfg!\n", vfs_sync->sync_task.domain);
			free(vfs_sync);
			return;
		}
		if(find_ip_stat(str2ip(ipinfo->s_ip), &peer))
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ip [%s] not on line !\n", ipinfo->s_ip);
			active_connect(ipinfo);
			list_add_tail(&(vfs_sync->list), &sync_list);
			return;
		}
	}
	LOG(vfs_sig_log, LOG_NORMAL, "start vfs_sync %d/%d\n", d1, d2);
	do_sub_sync(vfs_sync, peer);
}

static int init_sync_list()
{
	int i = 0;
	time_t maxtime = 0;
	time_t cur = time(NULL);
	t_vfs_sync_list *vfs_sync;  
	for (i = 0; i < MAXDIR_FOR_CS; i++)
	{
		if (self_ipinfo.dirs[i][0] == 0x0)
			break;
		int d1 = atoi(self_ipinfo.dirs[i]);
		char *t = strchr(self_ipinfo.dirs[i], '/');
		if (t == NULL)
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ERROR dir format [%s]\n", self_ipinfo.dirs[i]);
			continue;
		}
		t++;
		int d2 = atoi(t);
		int domain = 0;
		while (1)
		{
			if (self_ipinfo.archive)
				domain = get_next_fcs(domain, self_ipinfo.archive_isp);
			else
				domain = get_next_fcs(domain, ISP_FCS);
			if (domain == -1)
				break;
			time_t last = get_cs_time_stamp_by_int(d1, d2, domain);
			if (last <= 0)
				last = get_cs_dir_lasttime(d1, d2, domain);
			if (last >= maxtime)
				maxtime = last;
			vfs_sync = malloc(sizeof(t_vfs_sync_list));
			if (vfs_sync == NULL)
			{
				LOG(vfs_sig_log_err, LOG_ERROR, "ERROR malloc %m\n");
				return -1;
			}
			memset(vfs_sync, 0, sizeof(t_vfs_sync_list));
			INIT_LIST_HEAD(&(vfs_sync->list));
			vfs_sync->sync_task.starttime = last;
			vfs_sync->sync_task.endtime = cur;
			vfs_sync->sync_task.d1 = d1;
			vfs_sync->sync_task.d2 = d2;
			vfs_sync->sync_task.type = TASK_ADDFILE;
			snprintf(vfs_sync->sync_task.domain, sizeof(vfs_sync->sync_task.domain), "fcs%d.56.com", domain);
			LOG(vfs_sig_log, LOG_NORMAL, "gen sync task %d %d %s %ld %s\n", d1, d2, vfs_sync->sync_task.domain, last, ctime(&last));
			list_add_head(&(vfs_sync->list), &sync_list);
		}
	}

	if (maxtime == 0)
		maxtime = time(NULL) - 86400;

	int domain = 0;
	while (1)
	{
		domain = get_next_fcs(domain, ISP_FCS);
		if (domain == -1)
			break;
		vfs_sync = malloc(sizeof(t_vfs_sync_list));
		if (vfs_sync == NULL)
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "ERROR malloc %m\n");
			return -1;
		}
		memset(vfs_sync, 0, sizeof(t_vfs_sync_list));
		INIT_LIST_HEAD(&(vfs_sync->list));
		vfs_sync->sync_task.starttime = maxtime;
		vfs_sync->sync_task.type = TASK_DELFILE;
		snprintf(vfs_sync->sync_task.domain, sizeof(vfs_sync->sync_task.domain), "fcs%d.56.com", domain);
		LOG(vfs_sig_log, LOG_NORMAL, "gen sync task %s %s\n", vfs_sync->sync_task.domain, ctime(&maxtime));
		list_add_head(&(vfs_sync->list), &sync_list);
	}
	return 0;
}
