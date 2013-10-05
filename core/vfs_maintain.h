/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef __MAINTAIN_H__
#define __MAINTAIN_H__

#include"parsepara.h"
#include "pro_voss.h"
#include <stdarg.h>
enum {M_ONLINE = 0, M_OFFLINE, M_GETINFO, M_SYNCDIR, M_SYNCFILE, M_CONFUPDA, M_SETDIRTIME, M_GETDIRTIME, M_DELFILE, INVALID};
void do_request(int fd, int datalen, char *data);

#endif
