/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

void  sync_dir_thread(void * arg)
{
	t_vfs_tasklist *task = NULL;
	int ret = 0;
	while (1)
	{
		ret = vfs_get_task(&task, TASK_Q_SYNC_DIR_REQ);
		if (ret != GET_TASK_OK)
		{
			sleep(5);
			continue;
		}
		t_task_base *base = (t_task_base*) &(task->task.base);
		task->task.user = malloc(MAXSYNCBUF + sizeof(t_vfs_sig_head) + sizeof(t_vfs_sync_task));
		if (task->task.user == NULL)
		{
			LOG(vfs_sig_log_err, LOG_ERROR, "malloc error %d:%m\n", MAXSYNCBUF);
			base->overstatus = OVER_MALLOC;
			vfs_set_task(task, TASK_Q_SYNC_DIR_RSP);
			continue;
		}
		do_sync_dir_req_sub(task->task.user, base);
		vfs_set_task(task, TASK_Q_SYNC_DIR_RSP);
	}
}
