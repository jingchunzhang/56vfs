/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _56CDC_HTTP_H_
#define _56CDC_HTTP_H_
#include "list.h"
#include <stdint.h>
#include <sys/types.h>
#include <dirent.h>

typedef int (*http_request_cb)(char *req, char *o, int l);

typedef struct {
	char url[1024];
	time_t last;
	int fd;
	list_head_t alist;
} t_r_peer;

#endif
