#include "log.h"
#include "vfs_timer.h"
#include "list.h"
#include "global.h"
#include "util.h"
#include <stddef.h>

extern int glogfd;
static list_head_t delay_home;
static list_head_t delay_active;
static int default_max = 1024;

int delay_task_init()
{
	INIT_LIST_HEAD(&delay_home);
	INIT_LIST_HEAD(&delay_active);
	t_vfs_timer_list *timer_all = (t_vfs_timer_list *) malloc (sizeof(t_vfs_timer_list) * default_max);
	if (timer_all == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc timer_task error %m\n");
		return -1;
	}
	memset(timer_all, 0, sizeof(t_vfs_timer_list) * default_max);

	int i = 0;
	for (i = 0; i < default_max; i++)
	{
		INIT_LIST_HEAD(&(timer_all->tlist));
		list_add(&(timer_all->tlist), &delay_home);
		timer_all++;
	}
	return 0;
}

int add_to_delay_task(t_vfs_timer *vfs_timer)
{
	t_vfs_timer_list *timer = NULL;
	list_head_t *l;
	int get = 0;
	list_for_each_entry_safe_l(timer, l, &delay_home, tlist)
	{
		list_del_init(&(timer->tlist));
		get = 1;
		break;
	}
	if (get == 0)
		timer = (t_vfs_timer_list *)malloc(sizeof(t_vfs_timer_list));
	if (timer == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc err %m\n");
		return -1;
	}
	INIT_LIST_HEAD(&(timer->tlist));
	time_t cur = time(NULL);
	vfs_timer->next_time = cur + vfs_timer->span_time;   /*avoid time no sync*/
	memcpy(&(timer->vfs_timer), vfs_timer, sizeof(t_vfs_timer));
	LOG(glogfd, LOG_DEBUG, "args [%s] add into delay [%d] [%ld] [%ld]\n", timer->vfs_timer.args, timer->vfs_timer.span_time, timer->vfs_timer.next_time, cur);
	list_add(&(timer->tlist), &delay_active);
	return 0;
}

void scan_delay_task()
{
	t_vfs_timer_list *timer = NULL;
	list_head_t *l;
	time_t cur = time(NULL);
	t_vfs_timer *vfs_timer; 
	list_for_each_entry_safe_l(timer, l, &delay_active, tlist)
	{
		vfs_timer = &(timer->vfs_timer);
		if (cur > vfs_timer->next_time)
		{
			vfs_timer->cb(vfs_timer->args);
			if (vfs_timer->loop)
			{
				vfs_timer->next_time = cur + vfs_timer->span_time;
				continue;
			}
			list_del_init(&(timer->tlist));
			list_add(&(timer->tlist), &delay_home);
		}
	}
}

