/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _VFS_TASK_H_
#define _VFS_TASK_H_

/*
 *任务设计：业务线程消费后的任务数据放入 CLEAN任务队列，Agent线程定时清理该队列数据，放入HOME供业务线程循环消费
 *超时任务：有业务线程自己处理，最终落入CLEAN队列，有AGENT线程回收
 */

#include "list.h"
#include "vfs_init.h"
#include "nm_app_vfs.h"
#include <stdint.h>
#include <time.h>
#include <pthread.h>

#define SYNCIP 256
#define DOMAIN_PREFIX "fcs"

enum {OPER_IDLE, OPER_GET_REQ, OPER_GET_RSP, OPER_PUT, SYNC_2_GROUP};  /*任务类型 GET FCS-》CS， CS-》CS；PUT CS-》CS*/

enum {TASK_DELAY = 0, TASK_WAIT, TASK_WAIT_SYNC, TASK_WAIT_SYNC_IP, TASK_WAIT_TMP, TASK_Q_SYNC_DIR, TASK_Q_SYNC_DIR_TMP, TASK_RUN, TASK_FIN, TASK_CLEAN, TASK_HOME, TASK_SEND, TASK_RECV, TASK_SYNC_VOSS, TASK_Q_SYNC_DIR_REQ, TASK_Q_SYNC_DIR_RSP, TASK_UNKNOWN}; /*任务队列*/  /*add TASK_Q_SYNC_DIR_REQ TASK_Q_SYNC_DIR_RSP for thread sync dir */

enum {TASK_ADDFILE = '0', TASK_DELFILE, TASK_LINKFILE, TASK_SYNCDIR};  /* 任务类型 */

enum {OVER_UNKNOWN = 0, OVER_OK, OVER_E_MD5, OVER_PEERERR, TASK_EXIST, OVER_PEERCLOSE, OVER_UNLINK, OVER_TIMEOUT, OVER_MALLOC, OVER_SRC_DOMAIN_ERR, OVER_SRC_IP_OFFLINE, OVER_E_OPEN_SRCFILE, OVER_E_OPEN_DSTFILE, OVER_E_IP, OVER_E_TYPE, OVER_SEND_LEN, OVER_TOO_MANY_TRY, OVER_DISK_ERR, OVER_LAST};  /*任务结束时的状态*/

enum {GET_TASK_ERR = -1, GET_TASK_OK, GET_TASK_NOTHING};  /*从指定队列取任务的结果*/

enum {TASK_DST = 0, TASK_SOURCE, TASK_SRC_NOSYNC, TASK_SYNC_ISDIR, TASK_SYNC_VOSS_FILE}; /*本次任务是否需要相同组机器同步 */

extern const char *task_status[TASK_UNKNOWN]; 
extern const char *over_status[OVER_LAST]; 
typedef struct {
	char filename[256];
	char tmpfile[256];
	char linkfile[256];
	char filemd5[33];
	char src_domain[16];
	off_t offsize;
	uint32_t dstip;    /*本次任务目标ip，在GET或者PUT_FROM时，此ip是本机ip*/
	time_t starttime;
	time_t mtime;
	time_t ctime;
	off_t fsize;
	char okindex;
	char type;         /*文件变换类型，删除还是新增*/
	int8_t retry;     /*任务执行失败时，根据配置是否执行重新发起任务，已经重试次数，不能超过设定重试次数*/
	uint8_t overstatus; /*结束状态*/
	uint8_t isp;     /*voss sync 使用*/
	mode_t fmode;   /*stupid 为了兼容旧版本，没法对齐了*/
//	char bk[7];
}t_task_base;

typedef struct {
	off_t processlen; /*需要获取或者发送的数据长度*/
	off_t lastlen; /*上一个周期 处理的长度，初始化为0 */
	time_t   lasttime; /*上个周期时间戳*/
	time_t   starttime; /*开始时间戳*/
	time_t	 endtime; /*结束时间戳*/
	char peerip[16];  /*对端ip*/
	uint8_t oper_type; /*是从FCS GET文件，还是向同组CS  GET文件 */
	uint8_t need_sync; /*TASK_SOURCE：本次任务源头，TASK_DST：本次任务目的之一 */
	uint8_t sync_dir;  /**/
	uint8_t isp;      /**/
	uint8_t archive_isp;      /**/
	uint8_t bk[3];
}t_task_sub;

typedef struct {
	t_task_base base;
	t_task_sub  sub;
	void *user;
}t_vfs_taskinfo;

typedef struct {
	t_vfs_taskinfo task;
	list_head_t llist;
	list_head_t hlist;
	list_head_t userlist;
	uint32_t upip;
	uint8_t status;
	uint8_t bk[3];
} t_vfs_tasklist;

typedef struct {
	char domain[64];
	int d1;
	int d2;
	uint32_t ip;
	time_t task_stime; /*for time_out */
	time_t starttime; /*同步开始时间点 对CS来说，填写 目录上次同步的时间戳，对FCS来说，填写 0*/ 
	time_t endtime; /*同步结束时间点，对CS来说 填写 0，对FCS来说，填写FCS最近一次启动的时间戳 */ 
	char type;  /*ADDFILE, DELFILE 删除同步，只有CS才会发出请求，FCS不会发出删除同步*/
} t_vfs_sync_task;

typedef struct {
	uint8_t trycount;
	t_vfs_sync_task sync_task;
	list_head_t list;
} t_vfs_sync_list;

typedef void (*timeout_task)(t_vfs_tasklist *task);

int vfs_get_task(t_vfs_tasklist **task, int status);

int vfs_set_task(t_vfs_tasklist *task, int status);

int init_task_info();

int add_task_to_alltask(t_vfs_tasklist *task);

int check_task_from_alltask(t_task_base *base, t_task_sub *sub);

int get_task_from_alltask(t_vfs_tasklist **task, t_task_base *base, t_task_sub *sub);

int get_timeout_task_from_alltask(int timeout, timeout_task cb);

int scan_some_status_task(int status, timeout_task cb);

int get_task_count(int status);

void do_timeout_task();

void report_2_nm();

#endif
