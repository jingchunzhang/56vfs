/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_tmp_status.h"
#include "vfs_task.h"
#include "log.h"
#include "common.h"
#include "vfs_timer.h"
#include "c_api.h"
#include <stdlib.h>
extern int glogfd;

#define TASK_HASHSIZE 16
#define TASK_MOD 0x0F

atomic_t taskcount[TASK_UNKNOWN];
static list_head_t alltask[TASK_HASHSIZE];  /*总的任务，用来对任务进行快速定位，查找 ，为Tracker设计,排重*/

static pthread_mutex_t mutex[TASK_UNKNOWN];
static pthread_mutex_t TASK_ALL = PTHREAD_MUTEX_INITIALIZER;
static list_head_t vfstask[TASK_UNKNOWN];  /*sub task status ,just for cs or for tracker by cfg*/
uint64_t need_send_bytes;  /*等待发送的字节数*/
uint64_t been_send_bytes;  /*已经发送的字节数*/
extern t_ip_info self_ipinfo;

const char *task_status[TASK_UNKNOWN] = {"TASK_DELAY", "TASK_WAIT", "TASK_WAIT_SYNC", "TASK_WAIT_SYNC_IP", "TASK_WAIT_TMP", "TASK_Q_SYNC_DIR", "TASK_Q_SYNC_DIR_TMP", "TASK_RUN", "TASK_FIN", "TASK_CLEAN", "TASK_HOME", "TASK_SEND", "TASK_RECV", "TASK_SYNC_VOSS", "TASK_Q_SYNC_DIR_REQ", "TASK_Q_SYNC_DIR_RSP"};
const char *over_status[OVER_LAST] = {"OVER_UNKNOWN", "OVER_OK", "OVER_E_MD5", "OVER_PEERERR", "TASK_EXIST", "OVER_PEERCLOSE", "OVER_UNLINK", "OVER_TIMEOUT", "OVER_MALLOC", "OVER_SRC_DOMAIN_ERR", "OVER_SRC_IP_OFFLINE", "OVER_E_OPEN_SRCFILE", "OVER_E_OPEN_DSTFILE", "OVER_E_IP", "OVER_E_TYPE", "OVER_SEND_LEN", "OVER_TOO_MANY_TRY", "OVER_DISK_ERR"};
/*
 * every status have a list
 */

int vfs_get_task(t_vfs_tasklist **task, int status)
{
	int ret = GET_TASK_ERR;
	if (status < 0 || status >= TASK_UNKNOWN)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d status range error %d\n", FUNC, LN, status);
		return ret;
	}
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&mutex[status], &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return ret;
		}
	}

	ret = GET_TASK_NOTHING;
	t_vfs_tasklist *task0 = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(task0, l, &vfstask[status], llist)
	{
		atomic_dec(&(taskcount[status]));
		ret = GET_TASK_OK;
		*task = task0;
		(*task)->status = TASK_UNKNOWN;
		list_del_init(&(task0->llist));
		break;
	}
	if (ret == GET_TASK_NOTHING && status == TASK_HOME)
	{
		LOG(glogfd, LOG_DEBUG, "get from home , need malloc!\n");
		*task = (t_vfs_tasklist *) malloc(sizeof(t_vfs_tasklist));
		if (*task == NULL)
			LOG(glogfd, LOG_ERROR, "ERR %s:%d malloc %m\n", FUNC, LN);
		else
		{
			memset(*task, 0, sizeof(t_vfs_tasklist));
			ret = GET_TASK_OK;
			INIT_LIST_HEAD(&((*task)->llist));
			INIT_LIST_HEAD(&((*task)->hlist));
			INIT_LIST_HEAD(&((*task)->userlist));
			(*task)->status = TASK_UNKNOWN;
		}
	}

	if (pthread_mutex_unlock(&mutex[status]))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

int vfs_set_task(t_vfs_tasklist *task, int status)
{
	if (status < 0 || status >= TASK_UNKNOWN)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d status range error %d\n", FUNC, LN, status);
		return -1;
	}
	int ret = -1;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&mutex[status], &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d [%s] pthread_mutex_timedlock error %d\n", FUNC, LN, task_status[status], ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	list_del_init(&(task->llist));
	list_add_tail(&(task->llist), &vfstask[status]);
	task->status = status;
	atomic_inc(&(taskcount[status]));

	if (pthread_mutex_unlock(&mutex[status]))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

int init_task_info()
{
	int i = 0;
	for (i = 0; i < TASK_UNKNOWN; i++)
	{
		INIT_LIST_HEAD(&vfstask[i]);
		if (pthread_mutex_init(&mutex[i], NULL))
		{
			LOG(glogfd, LOG_ERROR, "pthread_mutex_init [%s] err %m\n", task_status[i]);
			report_err_2_nm(ID, FUNC, LN, 0);
			return -1;
		}
	}

	t_vfs_tasklist *taskall = (t_vfs_tasklist *) malloc (sizeof(t_vfs_tasklist) * 2048);
	if (taskall == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc t_vfs_tasklist error %m\n");
		return -1;
	}
	memset(taskall, 0, sizeof(t_vfs_tasklist) * 2048);

	for (i = 0; i < 2048; i++)
	{
		INIT_LIST_HEAD(&(taskall->llist));
		INIT_LIST_HEAD(&(taskall->hlist));
		INIT_LIST_HEAD(&(taskall->userlist));
		list_add_head(&(taskall->llist), &vfstask[TASK_HOME]);
		taskall->status = TASK_HOME;
		taskall++;
	}

	for (i = 0; i < TASK_HASHSIZE; i++)
	{
		INIT_LIST_HEAD(&(alltask[i]));
	}
	memset(taskcount, 0, sizeof(taskcount));
	t_vfs_timer vfs_timer;
	memset(&vfs_timer, 0, sizeof(vfs_timer));
	vfs_timer.loop = 1;
	vfs_timer.span_time = 60;
	vfs_timer.cb = report_2_nm;
	if (add_to_delay_task(&vfs_timer))
		LOG(glogfd, LOG_ERROR, "add_to_delay_task err %m\n");
	LOG(glogfd, LOG_DEBUG, "init_task_info ok!\n");
	return 0;
}

static int get_vfs_task_hash(t_task_base *base, t_task_sub *sub)
{
	char buf[1024] = {0x0};
	if (self_ipinfo.role == ROLE_CS)
		snprintf(buf, sizeof(buf), "%s|%s|%d|", base->filename, base->src_domain, base->type);
	else if (self_ipinfo.role == ROLE_FCS)
		snprintf(buf, sizeof(buf), "%s|%s|%d|", base->filename, base->src_domain, base->type);
	else if (self_ipinfo.role == ROLE_TRACKER)
		snprintf(buf, sizeof(buf), "%s|%s|%d|%u|", base->filename, base->src_domain, base->type, sub->isp);
	return r5hash(buf);
}

int add_task_to_alltask(t_vfs_tasklist *task)
{
	int index = get_vfs_task_hash(&(task->task.base), &(task->task.sub))&TASK_MOD; 
	int ret = -1;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&TASK_ALL, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	list_del_init(&(task->hlist));  /*avoid re add */

	list_add_head(&(task->hlist), &alltask[index]);
	if (pthread_mutex_unlock(&TASK_ALL))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return 0;
}

int check_task_from_alltask(t_task_base * base, t_task_sub *sub)
{
	int index = get_vfs_task_hash(base, sub)&TASK_MOD;
	t_vfs_tasklist *task0 = NULL;
	list_head_t *l;

	int ret = -1;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&TASK_ALL, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	ret = -1;
	list_for_each_entry_safe_l(task0, l, &alltask[index], hlist)
	{
		if (strcmp(base->filename, task0->task.base.filename))
			continue;
		if (strcmp(base->src_domain, task0->task.base.src_domain))
			continue;
		if (base->type != task0->task.base.type)
			continue;

		if (self_ipinfo.role == ROLE_TRACKER)
			if (sub->isp != task0->task.sub.isp)
				continue;

		ret = 0;
		break;
	}
	if (pthread_mutex_unlock(&TASK_ALL))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

int get_task_from_alltask(t_vfs_tasklist **task, t_task_base *base, t_task_sub *sub)
{
	int index = get_vfs_task_hash(base, sub)&TASK_MOD;
	t_vfs_tasklist *task0 = NULL;
	list_head_t *l;

	int ret = -1;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&TASK_ALL, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	ret = -1;
	list_for_each_entry_safe_l(task0, l, &alltask[index], hlist)
	{
		if (strcmp(base->filename, task0->task.base.filename))
			continue;
		if (strcmp(base->src_domain, task0->task.base.src_domain))
			continue;
		if (base->type != task0->task.base.type)
			continue;

		if (self_ipinfo.role == ROLE_TRACKER)
			if (sub->isp != task0->task.sub.isp)
				continue;

		ret = 0;
		list_del_init(&(task0->hlist));
		*task = task0;
		break;
	}
	if (pthread_mutex_unlock(&TASK_ALL))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

int get_timeout_task_from_alltask(int timeout, timeout_task cb)
{
	t_vfs_tasklist *task0 = NULL;
	list_head_t *l;

	int ret = -1;
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + time(NULL);
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&TASK_ALL, &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	time_t now = time(NULL);
	int index = 0;
	while (index <= TASK_MOD)
	{
		list_for_each_entry_safe_l(task0, l, &alltask[index], hlist)
		{
			if (now - task0->task.base.starttime < timeout)
				continue;
			list_del_init(&(task0->hlist));
			cb(task0);
		}
		index++;
	}
	if (pthread_mutex_unlock(&TASK_ALL))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

int scan_some_status_task(int status, timeout_task cb)
{
	if (status < 0 || status >= TASK_UNKNOWN)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d status range error %d\n", FUNC, LN, status);
		return -1;
	}
	int ret = -1;
	time_t cur = time(NULL);
	struct timespec to;
	to.tv_sec = g_config.lock_timeout + cur;
	to.tv_nsec = 0;
	ret = pthread_mutex_timedlock(&mutex[status], &to);
	if (ret != 0)
	{
		if (ret != EDEADLK)
		{
			LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_timedlock error %d\n", FUNC, LN, ret);
			report_err_2_nm(ID, FUNC, LN, ret);
			return -1;
		}
	}

	t_vfs_tasklist *task0 = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(task0, l, &vfstask[status], llist)
	{
		cb(task0);
	}

	if (pthread_mutex_unlock(&mutex[status]))
		LOG(glogfd, LOG_ERROR, "ERR %s:%d pthread_mutex_unlock error %m\n", FUNC, LN);
	return ret;
}

inline int get_task_count(int status)
{
	if (status < 0 || status >= TASK_UNKNOWN)
	{
		LOG(glogfd, LOG_ERROR, "ERR %s:%d status range error %d\n", FUNC, LN, status);
		return -1;
	}
	return atomic_read(&(taskcount[status]));
}

void report_2_nm()
{
	char buf[256] = {0x0};
	snprintf(buf, sizeof(buf), "vfs_test=%u|task_timeout=%ld|task_run=%d|task_wait=%d|task_fin=%d|task_clean=%d|task_wait_sync=%d|task_wait_sync_ip=%d|task_wait_tmp=%d|", g_config.vfs_test, g_config.task_timeout, get_task_count(TASK_RUN), get_task_count(TASK_WAIT), get_task_count(TASK_FIN), get_task_count(TASK_CLEAN), get_task_count(TASK_WAIT_SYNC), get_task_count(TASK_WAIT_SYNC_IP), get_task_count(TASK_WAIT_TMP)); 
	SetStr(VFS_TASK_COUNT, buf);

	int rulebase = VFS_TASK_DEPTH_BASE;
	int totaltask = 0;
	int i = 0;
	for (i = TASK_DELAY ; i < TASK_HOME; i++)
	{
		totaltask += get_task_count(i);
		SetInt(rulebase, get_task_count(i));
		rulebase++;
	}
	SetInt(VFS_TASK_COUNT_INT, totaltask);
	LOG(glogfd, LOG_NORMAL, "report 2 nm %s  %d\n", buf, totaltask);
}

void check_task_timeout(t_vfs_tasklist *task)
{
	time_t cur = time(NULL);
	t_task_base *base = &(task->task.base);
	if (cur - base->starttime > g_config.task_timeout)
		base->overstatus = OVER_TIMEOUT;

	if (task->status != TASK_CLEAN && base->overstatus == OVER_TIMEOUT)
	{
		LOG(glogfd, LOG_DEBUG, "move task [%s:%s:%s:%s] to home\n", base->src_domain, base->filename, over_status[base->overstatus%OVER_LAST], task_status[task->status%TASK_UNKNOWN]);
		list_del_init(&(task->llist));
		list_del_init(&(task->hlist));
		list_del_init(&(task->userlist));
		if (task->status != TASK_CLEAN && task->task.user &&(ROLE_CS == self_ipinfo.role || ROLE_TRACKER == self_ipinfo.role))
		{
			t_tmp_status *tmp = task->task.user;
			set_tmp_blank(tmp->pos, tmp);
			task->task.user = NULL;
		}
		atomic_dec(&(taskcount[task->status]));
		memset(&(task->task), 0, sizeof(task->task));
		vfs_set_task(task, TASK_HOME);
	}
}

void do_timeout_task()
{
	int i = 0;
	for (i = TASK_DELAY ; i < TASK_CLEAN; i++)
	{
		if (i == TASK_DELAY && self_ipinfo.role != ROLE_CS)
			continue;
		scan_some_status_task(i, check_task_timeout);
	}
	if (self_ipinfo.role == ROLE_CS)
		scan_some_status_task(TASK_SYNC_VOSS,  check_task_timeout);
}

