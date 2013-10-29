/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _CDC_HTTP_DB_H_
#define _CDC_HTTP_DB_H_

int init_db();

int get_ip_list_db(uint32_t *uips, int maxlen, char *d, char *f);

#endif
