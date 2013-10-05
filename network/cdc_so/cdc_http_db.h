/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef _CDC_HTTP_DB_H_
#define _CDC_HTTP_DB_H_

int init_db();

int get_ip_list_db(uint32_t *uips, int maxlen, char *d, char *f);

#endif
