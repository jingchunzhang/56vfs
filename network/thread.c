/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <alloca.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <sys/wait.h>
#include <sched.h>
#include <pthread.h>
#include <dlfcn.h>
#include <features.h>
#include <linux/unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/prctl.h>
#include "myconfig.h"
#include "thread.h"
#include "daemon.h"
#include "util.h"

static int curid = 0;
static int maxid;
struct thread mainthread;
static struct thread **threadlist;
/* require TLS suport */
static __thread int mythreadid;

int register_thread(const char *name, int (*entry)(void *), void *arg) {
	struct thread *th;

	if(curid >= maxid) {
	    printf("%s: Too many thread registered\n", name);
	    return -1;
	}

	th = malloc(sizeof(struct thread));
	if(th == NULL) 
		return -ENOMEM;
	threadlist[curid] = th;

	strncpy(th->name, name, 15);
	th->name[15] = '\0';
	th->entry = entry;
	th->arg = arg;
	th->id = curid;
	th->pid = 0;
	th->tid = 0;
	th->rv = -1;
	th->state = THREAD_STOPPING;
	curid++;
	return 0;
}
static int thread_entry(struct thread *th) {
	mythreadid = th->id;
#ifndef PR_SET_NAME
#define PR_SET_NAME 15
#endif
	prctl(PR_SET_NAME, th->name, 0, 0, 0);
	th->pid = syscall(SYS_gettid);
	th->state = THREAD_RUNNING;
	th->rv = th->entry(th->arg);
	th->state = THREAD_EXITED;
	pthread_exit(&th->rv);
	return th->rv;
}
/*struct thread *get_thread_by_pid(int pid) {
	int i;

	if(pid == 0) 
		pid = syscall(SYS_gettid);
	
	if(threadlist==NULL)
		return &mainthread;
	for(i = 0; i < curid; i++) {
		if(threadlist[i]->pid == pid)
			return threadlist[i];
	}
	return NULL;
}*/
int get_thread_id() { 
	return mythreadid; 
}
struct thread *get_my_thread(void) {
	return mythreadid == 0 ? &mainthread : threadlist[mythreadid];
}
static int start_thread_posix(struct thread *th) {
   	pthread_attr_t attr;
   	pthread_t tid;
	int rc;

	pthread_attr_init(&attr);
	if((rc = pthread_create(&tid, &attr, (void*(*)(void*))thread_entry, th)) != 0)
	{
	    errno = rc;
	    printf("\7%s: pthread_create(): %m\n", th->name);
	    return -1;
	}
	th->tid = tid;
	return 0;
}

static void wait_thread_posix(struct thread *th) {
	void *addr = NULL;
	pthread_join(th->tid, &addr);
}
int init_thread(void) {
	
	/* main thread */
	strcpy(mainthread.name, "main");
	mainthread.state = THREAD_RUNNING;
	mainthread.pid = syscall(SYS_gettid);
	mythreadid = 0;

	maxid = 16;
	threadlist = (struct thread**)malloc(maxid * sizeof(struct thread*));
	if(threadlist == NULL) {
		printf("init_thread: malloc threadlist fail, maxid=%d, %m\n", maxid);
		return -ENOMEM;
	}	
	threadlist[curid++] = &mainthread;
	return 0;
}
int start_threads() {
	int i;
	int n = 0;

	for(i = 1; i < curid; i++) {
		threadlist[i]->state = THREAD_STARTING;
		if(start_thread_posix(threadlist[i]) < 0) {
		    printf("\7ERROR: Some threads cannot start, Exitting\n");
		    exit(-1);
		    return -ENOMEM;
		}
		n++;
	}
	return 0;
}
void stop_threads() {
	int i;

	for(i = 1; i < curid; i++) {
	    if(threadlist[i]->state == THREAD_RUNNING) {
			pthread_kill(threadlist[i]->tid, SIGALRM);
	    }
	}

	for(i = 1; i < curid; i++) {
		if(threadlist[i]->state == THREAD_RUNNING)
		    threadlist[i]->state = THREAD_STOPPING;
		if(threadlist[i]->state != THREAD_UNUSED && threadlist[i]->state != THREAD_STANDBY)
		    wait_thread_posix(threadlist[i]);
		threadlist[i]->state = THREAD_UNUSED;
	}
	
	curid = 1; /* only main thread exist */
}
void thread_jumbo_title(void) {
   	char *title = alloca(128);
	char *p;
	char *svcname;

	p = stpcpy(title, _NS_":");
	
	svcname = myconfig_get_value("solib_file");
	if(svcname) {
		svcname = rindex(svcname, '/');
		p = stpcpy(p, ++svcname);	
	}
	else {
		p = stpcpy(p, "static");
	}

	if(curid > 1) {
	    *p++ = ',';
	    p = stpcpy(p, threadlist[1]->name);
	}
	*p = '\0';
	daemon_set_title(title);
}
