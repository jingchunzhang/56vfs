/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_tmp_status.h"
#include "vfs_task.h"
#include "vfs_init.h"
#include <stddef.h>

static int tmp_status_fd = -1;

static int exist_tmp = 1;  /*默认tmp文件存在*/

static list_head_t hometmp;
static list_head_t idletmp;
extern t_ip_info self_ipinfo;
static int sub_init_tmp_status(char * tmpfile, off_t size)
{
	if (access(tmpfile, W_OK|F_OK|R_OK))
	{
		int fd = open(tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
		if (fd < 0)
		{
			LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile);
			return -1;
		}
		int i = 0;
		char b = 0x0;
		for (; i < size; i++)
		{
			if (write(fd, &b, sizeof(b) != sizeof(b)))
			{
				LOG(glogfd, LOG_ERROR, "write err %s %d %m\n", tmpfile, fd);
				close(fd);
				return -1;
			}
		}
		close(fd);
	}
	if (truncate(tmpfile, size))
	{
		LOG(glogfd, LOG_ERROR, "truncate %s err %m\n", tmpfile);
		return -1;
	}
	return 0;
}

static int add_tmp_to_task(t_vfs_tasklist *item)
{
	LOG(glogfd, LOG_NORMAL, "prepare add tmp task %s:%s\n", item->task.base.filename, item->task.base.src_domain);
	if (self_ipinfo.isp != TEL && CNC != self_ipinfo.isp)
	{
		LOG(glogfd, LOG_ERROR, "little isp add tmp task %s:%s\n", item->task.base.filename, item->task.base.src_domain);
		return 1;
	}
	t_task_base *task = &(item->task.base);
	t_task_sub *sub = &(item->task.sub);
	if (self_ipinfo.role == ROLE_CS)
	{
		if (item->task.sub.oper_type == OPER_GET_REQ)
		{
			if (check_localfile_md5(task, VIDEOFILE) == LOCALFILE_OK)
				return 1;
		}
	}
	t_vfs_tasklist *task0;
	if (vfs_get_task(&task0, TASK_HOME))
	{
		LOG(glogfd, LOG_ERROR, "vfs_get_task err %m\n");
		return -1;
	}
	memcpy(&(task0->task), &(item->task), sizeof(t_vfs_taskinfo));
	task0->upip = item->upip;
	if (self_ipinfo.role == ROLE_CS && item->task.sub.oper_type == SYNC_2_GROUP)
	{
		if (vfs_set_task(task0, TASK_WAIT_SYNC))
		{
			LOG(glogfd, LOG_ERROR, "vfs_set_task err %m\n");
			return -1;
		}
	}
	else if (self_ipinfo.role == ROLE_CS && sub->need_sync == TASK_SYNC_ISDIR)
	{
		if (vfs_set_task(task0, TASK_Q_SYNC_DIR))
		{
			LOG(glogfd, LOG_ERROR, "vfs_set_task err %m\n");
			return -1;
		}
	}
	else
	{
		if (vfs_set_task(task0, TASK_WAIT))
		{
			LOG(glogfd, LOG_ERROR, "vfs_set_task err %m\n");
			return -1;
		}
	}
	add_task_to_alltask(task0);
	set_task_to_tmp(task0);
	return 0;
}

int init_load_tmp_status()
{
	char *datadir = myconfig_get_value("vfs_path");
	if (datadir == NULL)
		datadir = "../data";
	char tmpfile[256] = {0x0};
	snprintf(tmpfile, sizeof(tmpfile), "%s/vfs_tmp_status", datadir);
	struct stat filestat;
	if (stat(tmpfile, &filestat))
	{
		exist_tmp = 0;
		LOG(glogfd, LOG_ERROR, "%s not exit init it!\n", tmpfile);
		if (sub_init_tmp_status(tmpfile, sizeof(t_vfs_tasklist) * DEFAULT_ITEMS))
			return -1;
	}

	tmp_status_fd = open(tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
	if (tmp_status_fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile);
		return -1;
	}
	int init_size = DEFAULT_ITEMS;
	char tmpfile0[256] = {0x0};
	int fd = -1;
	if (exist_tmp)
	{
		init_size = filestat.st_size / sizeof(t_tmp_status);
		if (init_size < DEFAULT_ITEMS)
			init_size = DEFAULT_ITEMS;
		snprintf(tmpfile0, sizeof(tmpfile0), "%s.tmp", tmpfile);
		char cmd[512] = {0x0};
		snprintf(cmd, sizeof(cmd), "cp %s %s", tmpfile, tmpfile0);
		system(cmd);
		if (sub_init_tmp_status(tmpfile0, init_size * sizeof(t_tmp_status)))
			return -1;
		fd = open(tmpfile0, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
		if (fd < 0)
		{
			LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile0);
			return -1;
		}
	}
	else
		fd = tmp_status_fd;

	INIT_LIST_HEAD(&hometmp);
	INIT_LIST_HEAD(&idletmp);
	t_tmp_status *tmpstatus = (t_tmp_status *) malloc(sizeof(t_tmp_status) * init_size);
	if (tmpstatus == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc %m\n");
		return -1;
	}
	memset(tmpstatus, 0, sizeof(t_tmp_status) * init_size);
	int i = 0;
	t_tmp_status *tmpsub = tmpstatus;
	for (i = 0; i < DEFAULT_ITEMS; i++)
	{
		INIT_LIST_HEAD(&(tmpsub->list));
		list_add_head(&(tmpsub->list), &hometmp);
		tmpsub++;
	}

	t_vfs_tasklist item;
	int n = 0;
	for (i = 0; i < init_size; i++)
	{
		off_t pos = i * sizeof(item);
		n = read(fd, &item, sizeof(item));
		if (n < sizeof(item))
			break;
		if (!exist_tmp || strlen(item.task.base.filename) == 0)
		{
			set_tmp_blank(pos, NULL);
			continue;
		}
		n = add_tmp_to_task(&item);
		if (n)
		{
			if (n < 0)
				LOG(glogfd, LOG_ERROR, "add_tmp_to_task [%s:%s] err %m!\n", item.task.base.filename, item.task.base.src_domain);
			set_tmp_blank(pos, NULL);
		}
	}
	if (exist_tmp)
	{
		close(fd);
		close(tmp_status_fd);
		if (rename(tmpfile0, tmpfile))
		{
			LOG(glogfd, LOG_ERROR, "rename %s to %s err %m\n", tmpfile0, tmpfile);
			return -1;
		}
		tmp_status_fd = open(tmpfile, O_CREAT | O_RDWR | O_LARGEFILE, 0644);
		if (tmp_status_fd < 0)
		{
			LOG(glogfd, LOG_ERROR, "open %s err %m\n", tmpfile);
			return -1;
		}
	}
	exist_tmp = 1;
	return 0;
}

int set_task_to_tmp(t_vfs_tasklist *tasklist)
{
	LOG(glogfd, LOG_NORMAL, "set_task_to_tmp %s:%s\n", tasklist->task.base.filename, tasklist->task.base.src_domain);
	t_tmp_status *tmp = NULL;
	tasklist->task.user = NULL;
	int ret = 0;
	list_head_t *l;
	list_for_each_entry_safe_l(tmp, l, &idletmp, list)
	{
		ret = lseek(tmp_status_fd, tmp->pos, SEEK_SET);
		if (ret < 0)
		{
			LOG(glogfd, LOG_ERROR, "lseek [%ld] err %m\n", tmp->pos);
			return -1;
		}
		ret = write(tmp_status_fd, tasklist, sizeof(t_vfs_tasklist));
		if (ret < 0)
			LOG(glogfd, LOG_ERROR, "write err %m\n");

		LOG(glogfd, LOG_DEBUG, "%s:%s:%ld\n", tasklist->task.base.filename, tasklist->task.base.src_domain, tmp->pos);
		list_del_init(&(tmp->list));
		tasklist->task.user = tmp;
		return 0;
	}

	char *datadir = myconfig_get_value("vfs_path");
	if (datadir == NULL)
		datadir = "../data";
	char tmpfile[256] = {0x0};
	snprintf(tmpfile, sizeof(tmpfile), "%s/vfs_tmp_status", datadir);
	struct stat filestat;
	if (fstat(tmp_status_fd, &filestat))
	{
		LOG(glogfd, LOG_ERROR, "fstat err %d %m!\n", tmp_status_fd);
		return -1;
	}
	if (sub_init_tmp_status(tmpfile, filestat.st_size + sizeof(t_vfs_tasklist)))
	{
		LOG(glogfd, LOG_ERROR, "re sub_init_tmp_status err %m!\n");
		return -1;
	}
	ret = lseek(tmp_status_fd, filestat.st_size, SEEK_SET);
	if (ret < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek [%ld] err %m\n", filestat.st_size);
		return -1;
	}
	ret = write(tmp_status_fd, tasklist, sizeof(t_vfs_tasklist));
	if (ret < 0)
		LOG(glogfd, LOG_ERROR, "write err %m\n");

	tmp = malloc(sizeof(t_tmp_status));
	if (tmp == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc err %m\n");
		return -1;
	}

	INIT_LIST_HEAD(&(tmp->list));
	tmp->pos = filestat.st_size;
	tasklist->task.user = tmp;
	LOG(glogfd, LOG_DEBUG, "%s:%s:%ld\n", tasklist->task.base.filename, tasklist->task.base.src_domain, tmp->pos);
	return 0;
}

void set_tmp_blank(off_t pos, t_tmp_status *tmp)
{
	if (pos < 0)
	{
		LOG(glogfd, LOG_ERROR, "err pos [%ld] err %m\n", pos);
		return;
	}
	int ret = lseek(tmp_status_fd, pos, SEEK_SET);
	if (ret < 0)
	{
		LOG(glogfd, LOG_ERROR, "lseek [%ld] err %m\n", pos);
		return;
	}

	t_vfs_tasklist tasklist;
	memset(&tasklist, 0, sizeof(tasklist));
	ret = write(tmp_status_fd, &tasklist, sizeof(tasklist));
	if (ret < 0)
		LOG(glogfd, LOG_ERROR, "write err %m\n");

	if (tmp == NULL)
	{
		list_head_t *l;
		int get = 0;
		list_for_each_entry_safe_l(tmp, l, &hometmp, list)
		{
			get = 1;
			list_del_init(&(tmp->list));
			break;
		}
		if (get == 0)
		{
			tmp = malloc(sizeof(t_tmp_status));
			if (tmp == NULL)
			{
				LOG(glogfd, LOG_ERROR, "malloc err %m\n");
				return;
			}
			memset(tmp, 0, sizeof(t_tmp_status));
		}
	}
	tmp->pos = pos;
	INIT_LIST_HEAD(&(tmp->list));
	list_add_head(&(tmp->list), &idletmp);
	LOG(glogfd, LOG_TRACE, "add to idle %ld\n", tmp->pos);
	return;
}

