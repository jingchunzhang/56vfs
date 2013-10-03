#ifndef __MAINTAIN_H__
#define __MAINTAIN_H__

#include"parsepara.h"
#include "pro_voss.h"
#include <stdarg.h>
enum {M_ONLINE = 0, M_OFFLINE, M_GETINFO, M_SYNCDIR, M_SYNCFILE, M_CONFUPDA, M_SETDIRTIME, M_GETDIRTIME, M_DELFILE, INVALID};
void do_request(int fd, int datalen, char *data);

#endif
