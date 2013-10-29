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

/*KEY = fileinfo + type*/
typedef struct 
{
	char file[256];  /*domain:abs_file*/
	char srcip[20];
	char dstip[20];
	char task_status[32];
	char *role;
	uint8_t type;
	uint8_t bk2;
	uint8_t bk;
	uint8_t bk1;
	uint32_t ip;
	time_t starttime;
	time_t mtime;  /*任务状态修改时间*/
	off_t fsize;
	list_head_t iplist;  /*ip all task list*/
	list_head_t tlist;   /* task all ip list */
}t_cdc_http_task;

typedef int (*http_request_cb)(char *req, char *o, int l);

#endif
