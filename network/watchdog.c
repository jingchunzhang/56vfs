/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <signal.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <stddef.h>
#include <sys/types.h>
#include <linux/unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/syscall.h>
#include <syslog.h>
#include "myconfig.h"
#include "daemon.h"
#include "thread.h"
#include "watchdog.h"

struct threadmap {
	union {
		struct {
			int mthreads;
			int nthreads;
			struct threadstat threadlist[0];
		};
		char padding[16<<10];
	};
};
static struct threadmap threadmap[1] __attribute__((__aligned__(4096), __section__(".bss.shared")));
static int deadlock_count = 1;
int watchdog_pid;

struct threadstat *get_threadstat(void) {
	int id = get_thread_id();
	if(id >= threadmap->mthreads) 
		return NULL;
	if(id >= threadmap->nthreads) 
		threadmap->nthreads = id + 1;
	struct threadstat *ts = threadmap->threadlist + id;
	
	ts->badcnt = 0;
	atomic_set(&ts->tickcnt, 1);
	ts->tid = syscall(SYS_gettid);
	return ts;
}

static void init_threadmap(void) {
	mmap(threadmap, sizeof(threadmap), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
	threadmap->mthreads = ((16<<10) - offsetof(struct threadmap,threadlist)) / sizeof(struct threadstat);
	threadmap->nthreads = 0;
	deadlock_count = myconfig_get_intval("deadlock_timeout", 60);
	if(deadlock_count < 10)
		deadlock_count = 10;
}

static void check_threadmap(void) {
	if(threadmap == NULL) 
		return;
	int i;
	for(i = 0; i < threadmap->nthreads; i++) {
		const int tid = threadmap->threadlist[i].tid;
		if(tid == 0) 
			continue;
		const int cnt = atomic_read(&threadmap->threadlist[i].tickcnt);
		atomic_sub(cnt, &threadmap->threadlist[i].tickcnt);
		if(cnt) 
			threadmap->threadlist[i].badcnt = 0;
		else if(threadmap->threadlist[i].badcnt >= deadlock_count) {
			threadmap->threadlist[i].badcnt++;
			
			syslog(LOG_USER | LOG_CRIT, "%s watchdog[%d] kill %d\n", _NS_, getpid(), threadmap->threadlist[i].tid);
			
			kill(tid, SIGKILL);
			sleep(1);
			kill(tid, SIGKILL);
		} 
		else {
			threadmap->threadlist[i].badcnt++;
		}
	}
}

static void handler10sec(int signo) {
	check_threadmap();
	alarm(10);
}

int start_watchdog(void) {
	int pid;
	int cpid;
	int status = 0;

	if((((long)threadmap | sizeof(threadmap)) & (getpagesize() - 1)) != 0)
	    return 0;

	init_threadmap();
	
	if(myconfig_get_intval("disable_watchdog", 0))
	    return 0;

	if((pid = fork()) == -1) {
	    printf("watchdog fork(): %m\n");
	    return -1;
	}

	if(pid == 0) {
	    watchdog_pid = getppid();
	  	mainthread.pid = getpid();
	    usleep(1000);
		return 0; /* main process */
	}
	
	/* watchdog process */
	daemon_set_title(_NS_"-watchdog");
	
	signal(SIGCHLD, SIG_DFL);
	signal(SIGSEGV, SIG_DFL);
	signal(SIGBUS, SIG_DFL);
	signal(SIGILL, SIG_DFL);
	signal(SIGFPE, SIG_DFL);
	signal(SIGABRT, SIG_DFL);
	signal(SIGUSR1, SIG_IGN); //dont catch this signal for reload config
	signal(SIGALRM, handler10sec);
	
	alarm(10);
	while((cpid = wait(&status)) != pid) {

	    if(cpid == -1 && errno == ECHILD) 
			break;

	    if(stop) 
			kill(pid, SIGTERM);
	}
	if(WIFEXITED(status))
		restart = 0;
			
	daemon_stop();
	exit(0);
	return 0;
}
