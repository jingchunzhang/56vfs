/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _THREAD_H_
#define _THREAD_H_
#include <pthread.h>

struct thread {
	char name[16];
	int (*entry)(void *);
	void *arg;							
	pthread_t tid;
	pid_t pid;
	unsigned short id;
	unsigned char state;
	unsigned char inuse;
	int rv;
};

enum {
	THREAD_UNUSED = 0,	/* slot not used */
	THREAD_STOPPED = 1,	/* not running */
	THREAD_STANDBY,		/* in standby list */
	THREAD_STARTING,	/* starting thread */
	THREAD_RUNNING,		/* busy running */
	THREAD_STOPPING,	/* stopping req */
	THREAD_EXITED,		/* exited */
	THREAD_KILLED		/* killed */
};
extern struct thread mainthread;

extern int init_thread(void);
extern int register_thread(const char *name, int(*entry)(void *), void *priv);
extern int start_threads(void);
extern void stop_threads(void);
extern void thread_jumbo_title(void);
extern struct thread *get_my_thread(void);
extern int get_thread_id();
#endif
