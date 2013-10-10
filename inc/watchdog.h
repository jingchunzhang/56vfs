/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef _WATCHDOG_H_
#define _WATCHDOG_H_

#include "atomic.h"

struct threadstat {
	int tid;			//线程ID
	atomic_t tickcnt;	//tick数目
	int badcnt;			//线程被判定为停滞状态的次数
};

extern int watchdog_pid;
static inline void thread_reached(struct threadstat *ts) {
	if(ts)
		atomic_inc(&ts->tickcnt);
}
extern struct threadstat *get_threadstat(void);
extern int start_watchdog(void);

#endif
