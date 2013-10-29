/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _56_VFS_SO_H_
#define _56_VFS_SO_H_

#include <stdint.h>
#include <stdio.h>
#include <time.h>


#define GSIZE 1073741824    /*sendfile max send once*/

typedef struct {
	char flag; /*是否需要设置sockbuff ，对信令线路采用默认设置，对数据链路采用配置文件设置 */
	char name[255];
	int port;
	int maxevent;
	int threadcount;
} t_thread_arg;

volatile extern int stop;		//1-服务器停止，0-服务器运行中

extern time_t vfs_start_time; 

int vfs_signalling_thread(void *arg);

void add_fd_2_efd(int fd);

void do_close(int fd);

void modify_fd_event(int fd, int events);

int get_client_data(int fd, char **data, size_t *len);

int consume_client_data(int fd, size_t len);

int set_client_data(int fd, char *buf, size_t len);

int set_client_fd(int fd, int lfd, size_t offset, size_t len);

#endif
