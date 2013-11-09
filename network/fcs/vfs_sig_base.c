/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_init.h"
#include "vfs_localfile.h"
#include <libgen.h>
#define MAXFILE 10240

typedef struct {
	ino_t inode;
	off_t size;
	mode_t mode;
	time_t mtime;
	time_t ctime;
	char file[256];
}fileinfo;

static int sortinode(const void *p1, const void *p2)
{
	fileinfo *s1 = (fileinfo *)p1;
	fileinfo *s2 = (fileinfo *)p2;

	return s2->inode > s1->inode;
}

typedef struct 
{
	list_head_t list;
	char fname[256];
	time_t itime;
} t_sync_dir_file;

static int get_lock(pthread_mutex_t *mutex)
{
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	int ret = pthread_mutex_timedlock(mutex, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(vfs_sig_log, LOG_ERROR, "ERR %s:%d pthread_rwlock_timedrdlock error %d\n", FUNC, LN, ret);
			return -1;
		}
	}
	return 0;
}

static int release_lock(pthread_mutex_t *mutex)
{
	if (pthread_mutex_unlock(mutex))
		LOG(vfs_sig_log, LOG_ERROR, "ERR %s:%d pthread_rwlock_unlock error %m\n", FUNC, LN);
	return 0;
}

static void add_to_sync_dir_list(char *fname)
{
	int d1, d2;
	if (get_dir1_dir2(fname, &d1, &d2))
	{
		LOG(vfs_sig_log, LOG_ERROR, "error filename %s\n", fname);
		return;
	}
	char buf[256] = {0x0};
	snprintf(buf, sizeof(buf), "%d/%d/%s", d1, d2, basename(fname)); 
	uint32_t index = r5hash(buf) & 0xFF;

	t_sync_dir_file *file = malloc(sizeof(t_sync_dir_file));
	if (file == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "error malloc filename %s\n", fname);
		return;
	}
	memset(file, 0, sizeof(t_sync_dir_file));
	INIT_LIST_HEAD(&(file->list));
	strcpy(file->fname, buf);
	file->itime = time(NULL);
	if (get_lock(&sync_dir_mutex))
	{
		free(file);
		return;
	}
	list_add_tail(&(file->list), &sync_dir_list[index]);
	release_lock(&sync_dir_mutex);
}

static int check_sync_dir_list(char *fname)
{
	int d1, d2;
	if (get_dir1_dir2(fname, &d1, &d2))
	{
		LOG(vfs_sig_log, LOG_ERROR, "error filename %s\n", fname);
		return -1;
	}
	char buf[256] = {0x0};
	snprintf(buf, sizeof(buf), "%d/%d/%s", d1, d2, basename(fname)); 
	uint32_t index = r5hash(buf) & 0xFF;

	list_head_t *hashlist = &(sync_dir_list[index]);
	t_sync_dir_file *file = NULL;
	list_head_t *l;
	if (get_lock(&sync_dir_mutex))
		return -1;
	list_for_each_entry_safe_l(file, l, hashlist, list)
	{
		if (strcmp(buf, file->fname) == 0)
		{
			LOG(vfs_sig_log, LOG_NORMAL, "it is a re_sync file %s\n", buf);
			list_del_init(&(file->list));
			free(file);
			release_lock(&sync_dir_mutex);
			return 0;
		}
	}
	release_lock(&sync_dir_mutex);
	return -1;
}

static void scan_sync_dir_list()
{
	int i = 0;
	for (; i <= 0xFF; i++)
	{
		list_head_t *hashlist = &(sync_dir_list[i]);
		t_sync_dir_file *file = NULL;
		list_head_t *l;
		if (get_lock(&sync_dir_mutex))
			return ;
		time_t cur = time(NULL);
		list_for_each_entry_safe_l(file, l, hashlist, list)
		{
			if (cur - file->itime < 360000)
				break;
			LOG(vfs_sig_log, LOG_NORMAL, "too long %s\n", file->fname);
			list_del_init(&(file->list));
			free(file);
		}
		release_lock(&sync_dir_mutex);
	}
}

static int find_ip_stat(uint32_t ip, vfs_fcs_peer **dpeer)
{
	list_head_t *hashlist = &(online_list[ALLMASK&ip]);
	vfs_fcs_peer *peer = NULL;
	list_head_t *l;
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

static void do_fin_task()
{
	t_vfs_tasklist *task = NULL;
	while (1)
	{
		if (vfs_get_task(&task, TASK_FIN))
			break;
		if (task->task.sub.need_sync == TASK_SYNC_ISDIR)
		{
			task->task.base.ctime = time(NULL);
			set_fcs_time_stamp(&(task->task.base));
			sync_para.total_synced_task++;
			if (sync_para.flag == 2 && sync_para.total_synced_task >= sync_para.total_sync_task)
				LOG(vfs_sig_log, LOG_NORMAL, "SYNC[%d:%d]\n", sync_para.total_synced_task, sync_para.total_sync_task);
			t_vfs_tasklist *task0 = NULL;
			if (get_task_from_alltask(&task0, &(task->task.base), &(task->task.sub)))
				LOG(vfs_sig_log, LOG_NORMAL, "file [%s] not set all task!\n", task->task.base.filename);
		}
		vfs_set_task(task, TASK_CLEAN);
	}

	while (1)
	{
		if (vfs_get_task(&task, TASK_Q_SYNC_DIR_RSP))
			return;
		vfs_fcs_peer *peer;
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

static void check_task()
{
	t_vfs_tasklist *tasklist;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&tasklist, TASK_WAIT_SYNC_IP);
		if (ret != GET_TASK_OK)
		{
			LOG(vfs_sig_log, LOG_TRACE, "vfs_get_task get notihng %d\n", ret);
			break;
		}
		vfs_set_task(tasklist, TASK_WAIT_SYNC);
	}

	int once = 0;
	while (1)
	{
		once++;
		if (once >= g_config.fcs_max_task)
		{
			LOG(vfs_sig_log, LOG_DEBUG, "g_config.fcs_max_task set %d , once %d\n", g_config.fcs_max_task, once);
			return;
		}
		ret = vfs_get_task(&tasklist, TASK_WAIT_SYNC);
		if(ret != GET_TASK_OK)
			return;
		t_task_base *task = &(tasklist->task.base);
		t_task_sub *sub = &(tasklist->task.sub);
		if (check_task_from_alltask(task, sub) == 0)
			LOG(vfs_sig_log, LOG_DEBUG, "%s:%s task running!\n", task->filename, task->src_domain);

		list_del_init(&(tasklist->userlist));

		uint16_t bodylen = sizeof(t_task_base);
		t_vfs_sig_body ob;	
		memset(&ob, 0, sizeof(ob));
		memcpy(ob.body, task, sizeof(t_task_base));
		char obuf[2048] = {0x0};

		int n = create_sig_msg(NEWTASK_REQ, TASK_DISPATCH, &ob, obuf, bodylen);
		int mintask = 0;

		list_head_t *l = NULL;	
		vfs_fcs_peer *peer = NULL;	
		vfs_fcs_peer *usepeer = NULL;	
		list_for_each_entry_safe_l(peer, l, &trackerlist, tlist) 	
		{
			if (peer->role != ROLE_TRACKER)
				continue;
			if (usepeer == NULL)
			{
				usepeer = peer;
				mintask = peer->taskcount;
				continue;
			}
			if (peer->taskcount <= mintask)
			{
				mintask = peer->taskcount;
				usepeer = peer;
			}
		}
		if (usepeer)
		{
			peer = usepeer;
			set_client_data(peer->fd, obuf, n);		
			peer->sock_stat = SEND_LAST;
			modify_fd_event(peer->fd, EPOLLOUT);

			char ipstr[16] = {0x0};			
			ip2str(ipstr, peer->ip);			
			LOG(vfs_sig_log, LOG_NORMAL, "filename [%s] srcdomain [%s] filesize[%ld] filemd5 [%s] filectime [%ld] type [%c], ip[%s]\n",task->filename, task->src_domain, task->fsize, task->filemd5, task->ctime, task->type, ipstr);

			//移动当前节点到运行队列
			vfs_set_task(tasklist, TASK_RUN);
			add_task_to_alltask(tasklist);
			list_add_head(&(tasklist->userlist), &(peer->tasklist));
			peer->taskcount++;
		}
		else
			vfs_set_task(tasklist, TASK_WAIT_SYNC_IP);
	}
}

static void do_sub_sync(t_vfs_sync_list *vfs_sync, vfs_fcs_peer *peer)
{
	char sip[16] = {0x0};
	ip2str(sip, peer->ip);
	int d1 = vfs_sync->sync_task.d1;
	int d2 = vfs_sync->sync_task.d2;
	LOG(vfs_sig_log, LOG_NORMAL, "%s:%s:%d  sync [%d:%d] to [%s]!\n", ID, FUNC, LN, d1, d2, sip);
	char obuf[2048] = {0x0};
	int n = create_sig_msg(SYNC_DIR_REQ, TASK_SYNC, (t_vfs_sig_body *)&(vfs_sync->sync_task), obuf, sizeof(vfs_sync->sync_task));
	set_client_data(peer->fd, obuf, n);
	modify_fd_event(peer->fd, EPOLLOUT);
	sync_para.flag = 1;
}

static void do_active_sync()
{
	static time_t last = 0;
	time_t cur = time(NULL);
	if (cur - last < 900)
		return;
	last = cur;
	if (get_task_count(TASK_WAIT) > 4096)
	{
		LOG(vfs_sig_log, LOG_DEBUG, "wait for sync %d %d\n", sync_para.total_sync_task, sync_para.total_synced_task);
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

	t_cs_dir_info cs;
	if (get_cs_info(d1, d2, &cs))
	{
		LOG(vfs_sig_log, LOG_ERROR, "get %d %d get_cs_info err!\n", d1, d2);
		list_add_head(&(vfs_sync->list), &sync_list);
		return;
	}
	vfs_fcs_peer *peer = NULL;
	get = 0;
	int j = 0;
	for (j = 0; j < cs.index; j++)
	{
		if (find_ip_stat(cs.ip[j], &peer))
		{
			LOG(vfs_sig_log, LOG_DEBUG, "ip[%u] not in active !\n", cs.ip[j]);
			continue;
		}
		get = 1;
		do_sub_sync(vfs_sync, peer);
	}
	if (get == 0)
		list_add_head(&(vfs_sync->list), &sync_list);
	else
		free(vfs_sync);
}

static int add_2_sync_list(int d1, int d2)
{
	t_vfs_sync_list *vfs_sync = malloc(sizeof(t_vfs_sync_list));
	if (vfs_sync == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "ERROR malloc %m\n");
		return -1;
	}
	memset(vfs_sync, 0, sizeof(t_vfs_sync_list));
	INIT_LIST_HEAD(&(vfs_sync->list));
	vfs_sync->sync_task.starttime = 0;
	vfs_sync->sync_task.endtime = fcs_start_time;
	vfs_sync->sync_task.d1 = d1;
	vfs_sync->sync_task.d2 = d2;
	vfs_sync->sync_task.type = TASK_ADDFILE;
	snprintf(vfs_sync->sync_task.domain, sizeof(vfs_sync->sync_task.domain), "%s", hostname);
	LOG(vfs_sig_log, LOG_NORMAL, "gen sync task %d %d %s %ld %s\n", d1, d2, vfs_sync->sync_task.domain, fcs_start_time, ctime(&fcs_start_time));
	list_add_head(&(vfs_sync->list), &sync_list);
	set_fcs_time_stamp_by_int(d1, d2, time(NULL));
	return 0;
}

static int init_sync_list()
{
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s/%s", g_config.path, path_dirs[PATH_INDIR]);
	char obuf[16] = {0x0};
	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(path)) == NULL) 
		LOG(vfs_sig_log, LOG_ERROR, "opendir [%s] %s err  %m\n", g_config.path, path);
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
				LOG(vfs_sig_log, LOG_ERROR, "openfile %s err  %m\n", file);
				continue;
			}
			while (fgets(obuf, sizeof(obuf), fp))
			{
				char *t = strchr(obuf, '/');
				if (t)
				{
					int d1 = atoi(obuf);
					int d2 = atoi(t+1);
					add_2_sync_list(d1, d2);
				}
				else
					LOG(vfs_sig_log, LOG_ERROR, "err dirs for sync %s", obuf);
				memset(obuf, 0, sizeof(obuf));
			}
			fclose(fp);

			char bkfile[256] = {0x0};
			snprintf(bkfile, sizeof(bkfile), "%s/%s/%s", g_config.path, path_dirs[PATH_BKDIR], dirp->d_name);
			if (rename(file, bkfile))
			{
				LOG(vfs_sig_log, LOG_ERROR, "rename [%s] [%s] err %m\n", file, bkfile);
				unlink(file);
			}
		}
		closedir(dp);
	}
	return 0;
}

static void process_dir_sync(char *path, time_t endtime, time_t starttime, char *o, int *ol)
{
	fileinfo *statbuf = malloc(sizeof(fileinfo) * MAXFILE);
	if (statbuf == NULL)
	{
		LOG(vfs_sig_log, LOG_ERROR, "malloc err %m\n");
		return;
	}
	memset (statbuf, 0, sizeof(fileinfo) * MAXFILE);
	fileinfo *statbuf0 = statbuf;
	int filecount = 0;

	DIR *dp;
	struct dirent *dirp;
	if ((dp = opendir(path)) == NULL) 
	{
		LOG(vfs_sig_log, LOG_ERROR, "opendir %s err  %m\n", path);
		free(statbuf);
		return;
	}
	LOG(vfs_sig_log, LOG_TRACE, "opendir %s ok \n", path);
	while((dirp = readdir(dp)) != NULL) 
	{
		if (dirp->d_name[0] == '.')
			continue;
		char file[256] = {0x0};
		snprintf(file, sizeof(file), "%s/%s", path, dirp->d_name);
		if (check_file_filter(dirp->d_name))
		{
			LOG(vfs_sig_log, LOG_TRACE, "old task filename %s check not ok!\n", dirp->d_name);
			continue;
		}

		struct stat filestat;
		if(stat(file, &filestat) < 0) 
		{
			LOG(vfs_sig_log, LOG_ERROR, "stat error,filename:%s\n", file);
			continue;
		}
		if (!S_ISREG(filestat.st_mode))
			continue;
		if (starttime && filestat.st_ctime < starttime)
			continue;
		if (endtime && filestat.st_ctime > endtime)
			continue;
		if (access(file, R_OK))
		{
			LOG(vfs_sig_log, LOG_ERROR, "file access error,filename:%s:%m\n", file);
			continue;
		}
		snprintf(statbuf0->file, sizeof(statbuf0->file), "%s", file);
		statbuf0->inode = filestat.st_ino;
		statbuf0->size = filestat.st_size;
		statbuf0->mtime = filestat.st_mtime;
		statbuf0->mode = filestat.st_mode;
		statbuf0->ctime = filestat.st_ctime - g_config.task_timeout;
		statbuf0++;
		filecount++;
		if (filecount >= MAXFILE)
		{
			LOG(vfs_sig_log, LOG_ERROR, "too many file match %d :%s\n", filecount, path);
			break;
		}
	}
	closedir(dp);
	qsort(statbuf, filecount, sizeof(fileinfo), sortinode);
	statbuf0 = statbuf;

	int i = 0;
	for ( i = 0; i < filecount; i++)
	{
		t_task_base base;
		memset(&base, 0, sizeof(base));
		snprintf(base.filename, sizeof(base.filename), "%s", statbuf0->file);
		snprintf(base.src_domain, sizeof(base.src_domain), "%s", hostname);
		base.fsize = statbuf0->size;
		base.mtime = statbuf0->mtime;
		base.ctime = statbuf0->ctime;
		base.fmode = statbuf0->mode;
		ino_t inode = statbuf0->inode;

		unsigned char md5[33] = {0x0};
		if (getfilemd5view(base.filename, md5))
		{
			LOG(vfs_sig_log, LOG_ERROR, "md5 error,filename:%s %m\n", base.filename);
			continue;
		}
		snprintf(base.filemd5, sizeof(base.filemd5), "%s", md5);
		base.type = TASK_ADDFILE;
		base.starttime = time(NULL);
		statbuf0++;
		if (inode == statbuf0->inode)
		{
			base.type = TASK_LINKFILE;
			snprintf(base.linkfile, sizeof(base.linkfile), "%s", statbuf0->file);
		}

		if (o)
		{
			char obuf[2048] = {0x0};
			memset(obuf, 0, sizeof(obuf));
			int n = create_sig_msg(NEWTASK_REQ, TASK_SYNC_DIR, (t_vfs_sig_body *)&base, obuf, sizeof(t_task_base));
			LOG(vfs_sig_log, LOG_NORMAL, "sync task %s:%c\n", base.filename, base.type);
			if (n + *ol >= MAXSYNCBUF)
			{
				LOG(vfs_sig_log, LOG_ERROR, "too many file %s\n", path);
				break;
			}
			o = mempcpy(o, obuf, n);
			*ol += n;
		}
		else
		{

			t_vfs_tasklist *vfs_task;
			int ret = vfs_get_task(&vfs_task, TASK_HOME);
			if(ret != GET_TASK_OK) 
			{
				LOG(vfs_sig_log, LOG_ERROR, "dispatch old task error %s:%d:%d\n", ID, FUNC, LN);
				continue;
			}
			memcpy(&(vfs_task->task.base), &base, sizeof(base));
			LOG(vfs_sig_log, LOG_NORMAL, "dispatch old task %s:%c\n", base.filename, base.type);
			vfs_set_task(vfs_task, TASK_WAIT_SYNC);	
		}
	}
	free(statbuf);
}

static void do_sync_dir_req_sub(char *o, t_task_base *sbase)
{
	sbase->fsize = 0;
	t_vfs_sync_task *task = (t_vfs_sync_task *) sbase;
	char *datadir = myconfig_get_value("vfs_fcs_datadir");
	if (datadir == NULL)
		datadir = "/home/webadm/htdocs/flvdownload";
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s/%d/%d", datadir, task->d1, task->d2);
	int ol = 0;
	process_dir_sync(path, task->endtime, task->starttime, o, &ol);
	char obuf[2048] = {0x0};
	int n = 0;
	n = create_sig_msg(SYNC_DIR_RSP, TASK_SYNC_DIR, (t_vfs_sig_body *)task, obuf, sizeof(t_vfs_sync_task));
	memcpy(o + ol, obuf, n);
	sbase->fsize = n + ol;
}

static void do_sub_old_task(d1, d2)
{
	time_t oval = get_fcs_time_stamp_by_int(d1, d2);
	if (oval <= 0)
	{
		LOG(vfs_sig_log, LOG_TRACE, "get %d %d time_stamp err %m\n", d1, d2);
		oval = 0;
	}
	if (oval == 0)
		oval = get_fcs_dir_lasttime(d1, d2) + 1 ;

	char *datadir = myconfig_get_value("vfs_fcs_datadir");
	if (datadir == NULL)
		datadir = "/home/webadm/htdocs/flvdownload";
	char path[256] = {0x0};
	snprintf(path, sizeof(path), "%s/%d/%d", datadir, d1, d2);
	if (self_stat == ON_LINE)
		process_dir_sync(path, vfs_start_time, oval, NULL, NULL);
	else
		process_dir_sync(path, time(NULL), oval, NULL, NULL);
}

static void init_old_task()
{
	int d1 = 0;
	for (d1 = 0; d1 < DIR1; d1++)
	{
		int d2 = 0;
		for (d2 = 0; d2 < DIR2; d2++)
		{
			do_sub_old_task(d1, d2);
		}
	}
}

void sync_dir_thread(void * arg)
{

#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
	prctl(PR_SET_NAME, "fcs_sync_dir", 0, 0, 0);

	t_vfs_tasklist *task = NULL;
	init_old_task();
	int ret = 0;
	while (1)
	{
		if (self_stat == UNKOWN_STAT)
		{
			init_old_task();
			self_stat = ON_LINE;
		}
		ret = vfs_get_task(&task, TASK_Q_SYNC_DIR_REQ);
		if (ret != GET_TASK_OK)
		{
			sleep(5);
			continue;
		}
		t_task_base *base = (t_task_base*) &(task->task.base);
		task->task.user = malloc(MAXSYNCBUF + sizeof(t_vfs_sig_head) + sizeof(t_vfs_sync_task));
		if (task->task.user == NULL)
		{
			LOG(vfs_sig_log, LOG_ERROR, "malloc error %d:%m\n", MAXSYNCBUF);
			base->overstatus = OVER_MALLOC;
			vfs_set_task(task, TASK_Q_SYNC_DIR_RSP);
			continue;
		}
		do_sync_dir_req_sub(task->task.user, base);
		vfs_set_task(task, TASK_Q_SYNC_DIR_RSP);
	}
}

