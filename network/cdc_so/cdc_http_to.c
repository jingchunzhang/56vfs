/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

static list_head_t *allto;

static int threadcount = 0;

int init_r_to()
{
	threadcount = myconfig_get_intval("http_threadcount", 4);
	allto = (list_head_t *) malloc (sizeof(list_head_t) * threadcount);
	if (allto == NULL)
	{
		LOG(cdc_r_log, LOG_ERROR, "malloc ERR %m!\n");
		return -1;
	}
	return 0;
}

void add_2_active(t_r_peer *peer)
{
	list_head_t *alllist = allto + syscall(SYS_gettid)%threadcount;
	INIT_LIST_HEAD(&(peer->alist));
	peer->last = time(NULL);
	list_add_head(&(peer->alist), alllist);
}

int init_p_to()
{
	list_head_t *alllist = allto + syscall(SYS_gettid)%threadcount;
	INIT_LIST_HEAD(alllist);
	return 0;
}

void scan_to()
{
	list_head_t *alllist = allto + syscall(SYS_gettid)%threadcount;
	time_t now = time(NULL);
	list_head_t *l;
	t_r_peer *peer;
	list_for_each_entry_safe_l(peer, l, alllist, alist)
	{
		if (peer == NULL)
			continue;
		if (now - peer->last > 3)
		{
			LOG(cdc_r_log, LOG_NORMAL, "timeout close %d\n", peer->fd);
			do_close(peer->fd);
			return;
		}
		return;
	}
}
