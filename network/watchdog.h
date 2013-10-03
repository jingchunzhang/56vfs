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
