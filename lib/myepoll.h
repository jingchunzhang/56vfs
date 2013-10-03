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
