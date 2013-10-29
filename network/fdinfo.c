/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <stdio.h>
#include <stdlib.h>
#include "myconfig.h"
#include "mybuff.h"
#include "fdinfo.h"

int maxfds = 4096; 
struct conn *acon = NULL;

int init_fdinfo(void) {
	
	maxfds = myconfig_get_intval("maxfds", 4096);

    struct rlimit rlim;
	rlim.rlim_cur = maxfds;
	rlim.rlim_max = maxfds;
	if(setrlimit(RLIMIT_NOFILE, &rlim) < 0) {
		printf("\7Cannot increase file descriptors limit to %d\n", maxfds);
		//return -1;
	}
	
	acon = (struct conn*)malloc(sizeof(struct conn) * maxfds);
	if(acon == NULL) {
		printf("init_conn: malloc conn fail, maxfds=%d, %m\n", maxfds);	
		return -ENOMEM;
	}
	memset(acon, 0, sizeof(struct conn) * maxfds);

	int i;
	for (i = 0; i < maxfds; i++)
	{
		mybuff_init(&(acon[i].send_buff));
		mybuff_init(&(acon[i].recv_buff));
	}
	return 0;
}
void fini_fdinfo(void) {
	if(acon)
		free(acon);	
}
