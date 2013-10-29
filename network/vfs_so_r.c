/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_so.h"
#include "vfs_init.h"
#include "solib.h"
#include "myepoll.h"
#include "thread.h"
#include "myconfig.h"
#include "fdinfo.h"
#include "global.h"
#include "mybuff.h"
#include "log.h"
#include "watchdog.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/sendfile.h>

#if __GNUC__ < 4
static inline void barrier(void) { __asm__ volatile("":::"memory"); }
#else
static inline void barrier(void) { __sync_synchronize (); }
#endif

static int epfd;
static int lfd;
static int maxevent;
static struct mylib solib;

static int sub_init_signalling(char *so)
{
	solib.handle = dlopen(so, RTLD_NOW);
	if (solib.handle == NULL)
	{
		LOG(glogfd, LOG_ERROR, "open %s err %s\n", so, dlerror());
		return -1;
	}
	solib.svc_init = (proc_init)dlsym(solib.handle, "svc_init");
	if (solib.svc_init)
	{
		if (solib.svc_init() < 0)
		{
			LOG(glogfd, LOG_ERROR, "svc_init ERROR %m!\n");
			return -1;
		}
	}
	else
	{
		LOG(glogfd, LOG_ERROR, "svc_init must be imp!\n");
		return -1;
	}
	solib.svc_pinit = (proc_init)dlsym(solib.handle, "svc_pinit");
	solib.svc_initconn = (proc_method)dlsym(solib.handle, "svc_initconn");
	solib.svc_recv = (proc_method)dlsym(solib.handle, "svc_recv");
	solib.svc_send = (proc_method)dlsym(solib.handle, "svc_send");
	solib.svc_finiconn = (proc_fini)dlsym(solib.handle, "svc_finiconn");
	solib.svc_timeout = (proc_timeout)dlsym(solib.handle, "svc_timeout" );
	solib.svc_send_once = (proc_method)dlsym(solib.handle, "svc_send_once" );
	if (solib.svc_recv && solib.svc_send)
		return 0;
	LOG(glogfd, LOG_ERROR, "svc_send and svc_recv must be imp!\n");
	return -1;
}

void add_fd_2_efd(int fd)
{
	fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);

	struct conn *curconn = &acon[fd];
	curconn->fd = fd;
	curconn->send_len = 0;
	memset(curconn->peerip, 0, sizeof(curconn->peerip));
	mybuff_reinit(&(curconn->send_buff));
	mybuff_reinit(&(curconn->recv_buff));
	uint32_t ip = getpeerip(fd);
	ip2str(curconn->peerip, ip);
	LOG(glogfd, LOG_DEBUG, "fd [%d] [%s]set ok %d\n", fd, curconn->peerip, curconn->fd);
	epoll_add(epfd, fd, EPOLLIN);
}

void modify_fd_event(int fd, int events)
{
	events = EPOLLIN|events;
	epoll_mod(epfd, fd, events);
	LOG(glogfd, LOG_DEBUG, "fd [%d] be modify!\n", fd);
}

int get_client_data(int fd, char **data, size_t *len)
{
	struct conn *curcon = &acon[fd];
	if(mybuff_getdata(&(curcon->recv_buff), data, len)) 
		return -1;
	return 0;
}

int consume_client_data(int fd, size_t len)
{
	struct conn *curcon = &acon[fd];
	mybuff_skipdata(&(curcon->recv_buff), len);
	return 0;
}

int set_client_data(int fd, char *buf, size_t len)
{
	struct conn *curcon = &acon[fd];
	mybuff_setdata(&(curcon->send_buff), buf, len);
	return 0;
}

int set_client_fd(int fd, int localfd, size_t offset, size_t len)
{
	struct conn *curcon = &acon[fd];
	mybuff_setfile(&(curcon->send_buff), localfd, offset, len);
	return 0;
}

static void accept_new()
{
	struct sockaddr_in addr;
	socklen_t len;
   	int fd = 0;

	while (1)
	{
	    fd = accept(lfd, (struct sockaddr *)&addr, (len = sizeof(addr), &len));
		if (fd < 0)
			break;
		if (fd >= maxfds)
		{
			LOG(glogfd, LOG_ERROR, "fd overflow ![%d] > [%d]\n", fd, maxfds);
			close(fd);
			continue;
		}
		if (solib.svc_initconn(fd))
		{
			LOG(glogfd, LOG_ERROR, "fd init err ![%d] %m\n", fd);
			close(fd);
			continue;
		}
		add_fd_2_efd(fd);
	}
}

void do_close(int fd)
{
	if (fd >= 0 && fd < maxfds)
	{
		struct conn *curcon = &acon[fd];
		if (curcon->fd < 0)
			LOG(glogfd, LOG_ERROR, "fd %d already be closed %s\n", fd, FUNC);

		LOG(glogfd, LOG_DEBUG, "%s:%s:%d close fd %d\n", ID, FUNC, LN, fd);
		if (solib.svc_finiconn)
			solib.svc_finiconn(fd);
		barrier();
		epoll_del(epfd, fd);
		curcon->fd = -1;
		close(fd);
	}
}

static void do_send(int fd)
{
	LOG(glogfd, LOG_TRACE, "%s:%s:%d\n", ID, FUNC, LN);
	int ret = SEND_ADD_EPOLLIN;
	int n = 0;
	struct conn *curcon = &acon[fd];
	if (curcon->fd < 0)
	{
		LOG(glogfd, LOG_DEBUG, "fd %d already be closed %s\n", fd, FUNC);
		return;
	}
	int localfd;
	off_t start;
	char* data;
	size_t len;
	if(!mybuff_getdata(&(curcon->send_buff), &data, &len)) 
	{
		LOG(glogfd, LOG_TRACE, "fd[%d] get len from data [%d]\n", fd, len);
		while (1)
		{
			n = send(fd, data, len, MSG_DONTWAIT | MSG_NOSIGNAL);
			if(n > 0) 
			{
				LOG(glogfd, LOG_TRACE, "fd[%d] send len %d, datalen %d\n", fd, n, len);
				mybuff_skipdata(&(curcon->send_buff), n);
				if (n < len)
					ret = SEND_ADD_EPOLLOUT;
				curcon->send_len += n;
			}
			else if(errno == EINTR) 
				continue;
			else if(errno == EAGAIN) 
				ret = SEND_ADD_EPOLLOUT;
			else
			{
				LOG(glogfd, LOG_ERROR, "%s:%s:%d fd[%d] send err %d:%d:%m\n", ID, FUNC, LN, fd, n, len);
				ret = SEND_CLOSE;
			}
			break;
		}
	}
	if(ret == SEND_ADD_EPOLLIN && !mybuff_getfile(&(curcon->send_buff), &localfd, &start, &len))
	{
		LOG(glogfd, LOG_TRACE, "fd[%d] get len from file [%d]\n", fd, len);
		size_t len1 = len > GSIZE ? GSIZE : len;
		while (1)
		{
			n = sendfile64(fd, localfd, &start, len1);
			if(n > 0) 
			{
				mybuff_skipfile(&(curcon->send_buff), n);
				LOG(glogfd, LOG_TRACE, "%s:%s:%d fd[%d] send len %d, datalen %d\n", ID, FUNC, LN, fd, n, len1);
				if(n < len) 
					ret = SEND_ADD_EPOLLOUT;
				curcon->send_len += n;
			}
			else if(errno == EINTR) 
				continue;
			else if(errno == EAGAIN) 
				ret = SEND_ADD_EPOLLOUT;
			else 
			{
				LOG(glogfd, LOG_ERROR, "%s:%s:%d fd[%d] send err %d:%d:%m\n", ID, FUNC, LN, fd, n, len);
				ret = SEND_CLOSE;
			}
			break;
		}
	}

	switch (ret)
	{
		case SEND_CLOSE:
			do_close(fd);
			return;
		case SEND_ADD_EPOLLIN:
			modify_fd_event(fd, EPOLLIN);
			break;
		case SEND_ADD_EPOLLOUT:
			modify_fd_event(fd, EPOLLOUT);
			break;
		case SEND_ADD_EPOLLALL:
			modify_fd_event(fd, EPOLLOUT|EPOLLIN);
			break;
		default:
			modify_fd_event(fd, EPOLLIN);
			break;
	}

	if (n > 0 && solib.svc_send_once)
		solib.svc_send_once(fd);

	if (ret == SEND_ADD_EPOLLIN && solib.svc_send)
		if (solib.svc_send(fd) == SEND_CLOSE)
		{
			LOG(glogfd, LOG_DEBUG, "%s:%s:%d send close %d\n", ID, FUNC, LN, fd);
			do_close(fd);
		}
}

static void do_recv(int fd)
{
	struct conn *curcon = &acon[fd];
	if (curcon->fd < 0)
	{
		LOG(glogfd, LOG_ERROR, "fd %d already be closed %s\n", fd, FUNC);
		return;
	}

	char iobuf[20480] = {0x0};
	int n = -1;
	while (1)
	{
		n = recv(fd, iobuf, sizeof(iobuf), MSG_DONTWAIT);
		if (n > 0)
		{
			LOG(glogfd, LOG_DEBUG, "fd[%d] recv len %d\n", fd, n);
			mybuff_setdata(&(curcon->recv_buff), iobuf, n);
			if (n == sizeof(iobuf))
			{
				LOG(glogfd, LOG_DEBUG, "fd[%d] need recv nextloop %d\n", fd, n);
				continue;
			}
			break;
		}
		if (n == 0)
		{
			LOG(glogfd, LOG_ERROR, "fd[%d] close %s:%d!\n", fd, ID, LN);
			return do_close(fd);
		}
		if (errno == EINTR)
		{
			LOG(glogfd, LOG_TRACE, "fd[%d] need recv again!\n", fd);
			continue;
		}
		else if (errno == EAGAIN)
		{
			LOG(glogfd, LOG_TRACE, "fd[%d] need recv next!\n", fd);
			modify_fd_event(fd, EPOLLIN);
			break;
		}
		else
		{
			LOG(glogfd, LOG_ERROR, "fd[%d] close %s:%d!\n", fd, ID, LN);
			return do_close(fd);
		}
	}

	if (n <= 0)
		return;

	int ret = solib.svc_recv(fd);
	switch (ret)
	{
		case RECV_CLOSE:
			LOG(glogfd, LOG_ERROR, "fd[%d] close %s:%d!\n", fd, ID, LN);
			do_close(fd);
			break;
		case RECV_SEND:
			do_send(fd);
			break;
		case RECV_ADD_EPOLLIN:
			modify_fd_event(fd, EPOLLIN);
			break;
		case RECV_ADD_EPOLLOUT:
			modify_fd_event(fd, EPOLLOUT);
			break;
		case RECV_ADD_EPOLLALL:
			modify_fd_event(fd, EPOLLOUT|EPOLLIN);
			break;
		default:
			modify_fd_event(fd, EPOLLIN);
			break;
	}
}

static void do_process(int fd, int events)
{
	if(!(events & (EPOLLIN | EPOLLOUT))) 
	{
		LOG(glogfd, LOG_DEBUG, "error event %d, %d\n", events, fd);
		do_close(fd);
		return;
	}
	if(events & EPOLLIN) 
	{
		LOG(glogfd, LOG_TRACE, "read event %d, %d\n", events, fd);
		do_recv(fd);
	}
	if(events & EPOLLOUT) 
	{
		LOG(glogfd, LOG_TRACE, "send event %d, %d\n", events, fd);
		do_send(fd);
	}
}

static void set_socket_attr(int fd)
{
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &init_buff_size, sizeof(int));
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &init_buff_size, sizeof(int));
}

static void so_thread_entry(void *arg)
{
	pthread_detach(pthread_self());
	if (solib.svc_pinit)
	{
		if (solib.svc_pinit() < 0)
		{
			LOG(glogfd, LOG_ERROR, "svc_pinit ERROR %m!\n");
			stop = 1;
			return ;
		}
	}
	else
	{
		LOG(glogfd, LOG_ERROR, "svc_pinit must be imp!\n");
		stop = 1;
		return ;
	}
	int n = 0, i = 0;
	time_t last = time(NULL);
	time_t now = last;
	struct epoll_event *pev = (struct epoll_event*)malloc(sizeof(struct epoll_event) * maxevent);
	if(pev == NULL) 
	{
		LOG(glogfd, LOG_ERROR, "allocate epoll_event(%d): %m\n", maxevent);
		stop = 1;
		return ;
	}
	while (!stop)
	{
		n = epoll_wait(epfd, pev, maxevent, 1000);
		for(i = 0; i < n; i++) 
		{
			if (pev[i].data.fd == lfd)
				accept_new();
			else
				do_process(pev[i].data.fd, pev[i].events);
		}
		now = time(NULL);
		if (now > last + g_config.cktimeout)
		{
			last = now;
			if (solib.svc_timeout)
				solib.svc_timeout();
		}
	}
}

int vfs_signalling_thread(void *arg)
{
	t_thread_arg *argp = (t_thread_arg *)arg;
	if (sub_init_signalling(argp->name))
	{
		stop = 1;
		return -1;
	}
	if (argp->port > 0)
	{
		lfd = get_listen_sock(argp->port);
		if (lfd < 0)
		{
			LOG(glogfd, LOG_ERROR, "get_listen_sock err %d\n", argp->port);
			stop = 1;
			return -1;
		}
		LOG(glogfd, LOG_DEBUG, "%s listen on %d\n", argp->name, argp->port);
	}
	maxevent = argp->maxevent;
    epfd = epoll_create(maxevent);
	if(epfd < 0) 
	{
		LOG(glogfd, LOG_ERROR, "epoll_create(%d): %m\n", maxevent);
		stop = 1;
		return -1;
	}
	if (argp->port > 0)
	{
		if (argp->flag)
			set_socket_attr(lfd);
	}

	struct threadstat *thst = get_threadstat();
	int event = EPOLLIN;
	if (argp->port > 0)
		epoll_add(epfd, lfd, event);
	LOG(glogfd, LOG_DEBUG, "%s:%s:%d\n", ID, FUNC, LN);

	int i = 0;
   	pthread_attr_t attr;
   	pthread_t tid;
	pthread_attr_init(&attr);
	int rc;

	if (argp->threadcount <= 0)
		argp->threadcount = 1;
	for (; i < argp->threadcount; i++)
	{
		usleep(5000);
		if((rc = pthread_create(&tid, &attr, (void*(*)(void*))so_thread_entry, NULL)) != 0)
		{
			LOG(glogfd, LOG_ERROR, "pthread_create err %m\n");
			stop = 1;
			return -1;
		}
	}
	while (!stop)
	{
		thread_reached(thst);
		sleep(200);
	}
	return 0;
}


