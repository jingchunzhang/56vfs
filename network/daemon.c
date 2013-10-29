/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <syslog.h>
#include "daemon.h"
#include "myconfig.h"

//由于syslog.h里面定义了LOG_DEBUG，所以这里要undef，否则会编译告警
#ifdef LOG_DEBUG
#undef LOG_DEBUG
#endif
#include "log.h"

volatile int stop = 0;
volatile int restart = 1;
static char *progname = NULL;
static char **saved_argv = NULL;
static int saved_argc = 0;
static char *arg_start;
static char *arg_end;
static char *env_start;

static void sigterm_handler(int signo) {
    restart = 0;
    stop = 1;
}
static void sighup_handler(int signo) {
    restart = 1;
    stop = 1;
}
static void nop_handler(int signo) {
}
static void reload_handler(int signo) {
	myconfig_reload();
}
static void get_progname(void) {
    
    if(progname)
	    return;
    
	char p[4096];
    int n = readlink("/proc/self/exe", p, sizeof(p) - 1);
    if(n > 0) {
		p[n] = '\0';
		if(!strcmp(p + n - 10," (deleted)"))
	    	p[n - 10] = '\0';
		progname = strdup(p);
    }
}
static void dup_argv(int argc, char **argv) {
	get_progname();
	saved_argv = malloc(sizeof(char *) * (argc + 10));
	if(saved_argv == NULL)
		return;
	saved_argv[argc] = NULL;
	saved_argc = argc;
	while(--argc >= 0)
		saved_argv[argc] = strdup(argv[argc]);
}

static void free_argv(void) {
	char **argv;
	for(argv = saved_argv; *argv; argv++)
		free(*argv);
	free(saved_argv);
	saved_argv = NULL;
	saved_argc = 0;
}

int daemon_start(int argc, char **argv){
	
	if(!myconfig_get_intval("foreground", 0)) {	
		printf("Switching to background\n");
		if(getppid() != 1)
			daemon(1, 1);
	}

	struct sigaction sa;
	sigset_t sset;
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sigterm_handler;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);

	sa.sa_handler = nop_handler;
	sigaction(SIGIO, &sa, NULL);
	sigaction(SIGALRM, &sa, NULL);

	sa.sa_handler = sighup_handler;
	sigaction(SIGHUP, &sa, NULL);
	
	sa.sa_handler = reload_handler;
	sigaction(SIGUSR1, &sa, NULL);

	signal(SIGPIPE, SIG_IGN);
	signal(SIGXFSZ, SIG_IGN);
	signal(SIGCHLD, SIG_DFL);

	sigemptyset(&sset);
	sigaddset(&sset, SIGSEGV);
	sigaddset(&sset, SIGBUS);
	sigaddset(&sset, SIGABRT);
	sigaddset(&sset, SIGILL);
	sigaddset(&sset, SIGCHLD);
	sigaddset(&sset, SIGFPE);
	sigaddset(&sset, SIGHUP);
	sigaddset(&sset, SIGTERM);
	sigaddset(&sset, SIGINT);
	sigaddset(&sset, SIGQUIT);
	sigprocmask(SIG_UNBLOCK, &sset, &sset);

	arg_start = argv[0];
	arg_end = argv[argc - 1] + strlen(argv[argc - 1]) + 1;
	env_start = environ[0];

	dup_argv(argc, argv);

	if(progname == NULL || saved_argv == NULL) {
		printf("can't resolve executable path, restart not support\n");
		signal(SIGHUP, sigterm_handler);
	}

	return 0;
}

void daemon_stop(void) {
	if(restart && progname && saved_argv) {
		sleep(1);

		syslog(LOG_USER | LOG_CRIT, "%s to be restarted\n", _NS_);
		
		execv(progname, saved_argv);
		get_progname();
		if(progname)
			execv(progname, saved_argv);
	}
	free_argv();
	free(progname);
	progname = NULL;
}

void daemon_set_title(const char *title) {
    int tlen = strlen(title) + 1;
	int i;
	char *p;
	
	//防止新标题覆盖了环境变量的内存区
	if(arg_end - arg_start < tlen && env_start == arg_end) {
	    /* dup environs to get more space */
		char *env_end = env_start;
	    for(i = 0; environ[i]; i++) {
			if(env_end == environ[i]) {
		    	env_end = environ[i] + strlen(environ[i]) + 1;
		    	environ[i] = strdup(environ[i]);
			} 
			else
		    	break;
	    }
	    arg_end = env_end;
	    env_start = NULL;
	}
	i = arg_end - arg_start;
	if(tlen == i) {
	    strcpy(arg_start, title);
	} 
	else if(tlen < i) {
	    strcpy(arg_start, title);
	    memset(arg_start + tlen, 0, i - tlen);
	} 
	else {
	    *(char *)mempcpy(arg_start, title, i - 1) = '\0';
	}
	if(env_start) {
	    p = strchr(arg_start, ' ');
	    if(p) 
			*p = '\0';
	}
}
