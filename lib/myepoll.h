/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _56_VFS_EPOLL_H
#define _56_VFS_EPOLL_H
#include <stdint.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <netdb.h>
void epoll_add(int epfd, int fd, int events); 

void epoll_mod(int epfd, int fd, int events);

void epoll_del(int epfd, int fd);
	
int get_listen_sock(int port);

int createsocket(char *ip, int port);

#endif
