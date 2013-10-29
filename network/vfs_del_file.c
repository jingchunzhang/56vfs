/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_del_file.h"
extern t_ip_info self_ipinfo;

static void get_del_file(t_task_base *base, char *file)
{
	char * datadir = myconfig_get_value("vfs_deldir");
	if (datadir == NULL)
		datadir = "/diska/vfs/path/delfile/";
	if (self_ipinfo.role == ROLE_CS)
		sprintf(file, "%s/%s_del_list", datadir, base->src_domain);
	if (self_ipinfo.role == ROLE_FCS)
		sprintf(file, "%s/del_list", datadir);
}

int add_2_del_file (t_task_base *base)
{
	char file[256] = {0x0};
	get_del_file(base, file);
	int fd = open(file, O_CREAT | O_RDWR | O_LARGEFILE | O_APPEND, 0644);
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %m\n", file);
		return -1;
	}

	t_vfs_del_file del_file;
	memset(&del_file, 0, sizeof(del_file));
	snprintf(del_file.dtime, sizeof(del_file.dtime), "%ld", base->ctime);
	del_file.idle = ' ';
	snprintf(del_file.file, sizeof(del_file.file), "%s", base->filename);
	del_file.line = '\n';
	int ret = write(fd, &del_file, sizeof(del_file));
	if (ret < 0)
		LOG(glogfd, LOG_ERROR, "write err %m\n");
	close(fd);
	return 0;
}

int find_last_index(t_task_base *base, time_t last)
{
	char file[256] = {0x0};
	get_del_file(base, file);
	struct stat filestat;
	if (stat(file, &filestat))
	{
		LOG(glogfd, LOG_ERROR, "%s not exit init it!\n", file);
		return -1;
	}

	int fd = open(file, O_RDWR | O_LARGEFILE);
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "%s open err %m!\n", file);
		return -1;
	}

	t_vfs_del_file delfile;
	memset(&delfile, 0, sizeof(delfile));
	
	int index = -1;
	int flag = 0;
	while (1)
	{
		int n = read(fd, &delfile, sizeof(delfile));
		if (n <= 0)
			break;
		index++;
		if (atol(delfile.dtime) == last)
		{
			snprintf(base->filename, sizeof(base->filename), "%s", delfile.file);
			flag = 1;
			break;
		}
	}
	close(fd);
	if (flag)
		return index;
	return -1;
}

int get_from_del_file (t_task_base *base, int next, time_t cur)
{
	char file[256] = {0x0};
	get_del_file(base, file);
	struct stat filestat;
	if (stat(file, &filestat))
	{
		LOG(glogfd, LOG_ERROR, "%s not exit init it!\n", file);
		return -1;
	}

	t_vfs_del_file delfile;
	memset(&delfile, 0, sizeof(delfile));
	
	int max = filestat.st_size/sizeof(delfile);
	if (max <= next)
	{
		LOG(glogfd, LOG_NORMAL, "%s end!\n", file);
		return -1;
	}

	int fd = open(file, O_RDWR | O_LARGEFILE);
	if (fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "%s open err %m!\n", file);
		return -1;
	}

	off_t pos = sizeof(delfile) * next;
	lseek(fd, pos, SEEK_SET);
	int n = read(fd, &delfile, sizeof(delfile));
	if (n <= 0)
	{
		LOG(glogfd, LOG_ERROR, "%s read err %m!\n", file);
		close(fd);
		return -1;
	}
	if (atol(delfile.dtime) > cur)
	{
		close(fd);
		LOG(glogfd, LOG_NORMAL, "%s end!\n", file);
		return -1;
	}
	close(fd);
	return next;
}


