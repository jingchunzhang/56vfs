/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "hiredis.h"
#include "cdc_redis_api.h"
#include "hotkey.h"
#include "common.h"
#include "global.h"
#include "vfs_so.h"
#include "vfs_init.h"
#include "myepoll.h"
#include "protocol.h"
#include "util.h"
extern int cdc_r_log ;
__thread redisContext *c;

int redis_connect(char *ip, int port) 
{
	c = NULL;
	c = redisConnect(ip, port);
    if (!c || c->err)
	{
		if (c)
			LOG(cdc_r_log, LOG_ERROR, "Connection error: %s\n", c->errstr);
		else
			LOG(cdc_r_log, LOG_ERROR, "Connection error: %s:%d:%m\n", ip, port);
		return -1;
	}
    return 0;
}

int init_db()
{
	char *ip = myconfig_get_value("redis_server");
	if (ip == NULL)
	{
		LOG(cdc_r_log, LOG_ERROR, "config have not redis_server!\n");
		return -1;
	}
    return redis_connect(ip, myconfig_get_intval("redis_port", 16379));
}

int get_ip_list_db(uint32_t *uips, int maxlen, char *d, char *f)
{
	get_redis(d, f, uips, maxlen * sizeof(uint32_t), c);
	return 0;
}
