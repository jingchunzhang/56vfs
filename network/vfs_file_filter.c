/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "vfs_file_filter.h"
#include "list.h"
#include "log.h"
#include "global.h"
#include "util.h"
#include <stdio.h>
#include <stddef.h>

extern int glogfd;
static list_head_t allow_list;
static list_head_t deny_list;

typedef struct {
	list_head_t list;
	char strings[64];
	uint8_t s_len;
	char type;    /* 0, suffix, 1, prefix, 2, match */
} t_vfs_filter;

static int add_filter(char *s, int type, list_head_t *mlist)
{
	t_vfs_filter * filter = malloc(sizeof(t_vfs_filter));
	if (filter == NULL)
	{
		LOG(glogfd, LOG_ERROR, "malloc err %m\n");
		return -1;
	}
	memset(filter, 0, sizeof(t_vfs_filter));
	INIT_LIST_HEAD(&(filter->list));
	snprintf(filter->strings, sizeof(filter->strings), "%s", s);
	filter->type = type;
	filter->s_len = strlen(s);
	list_add_head(&(filter->list), mlist);
	LOG(glogfd, LOG_DEBUG, "add file filter %s\n", s);
	return 0;
}

static int sub_init_filter(char *s, int type, list_head_t *mlist)
{
	char *t = s;
	while (1)
	{
		t = strchr(s, '|');
		if (t == NULL)
			break;
		*t = 0x0;

		if (*s == '*')
			s++;
		if (*s == 0x0)
			continue;
		if (add_filter(s, type, mlist))
			return -1;
		t++;
		s = t;
	}
	if (*s == '*')
		s++;
	if (*s == 0x0)
		return 0;
	return add_filter(s, type, mlist);
}

int init_file_filter()
{
	INIT_LIST_HEAD(&allow_list);
	INIT_LIST_HEAD(&deny_list);
	char* pval = NULL; int i = 0;
	for( i = 0; ( pval = myconfig_get_multivalue( "file_allow_suffix", i ) ) != NULL; i++ )
	{
		if (strlen(pval) == 0)
			continue;
		if (sub_init_filter(pval, 0, &allow_list))
		{
			LOG(glogfd, LOG_ERROR, "sub_init_filter %s err %m\n", pval);
			return -1;
		}
	}
	return 0;
}

static int check_file_allow(char *file)
{
	int ret = 1;
	int f_len = strlen(file);
	t_vfs_filter *filter = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(filter, l, &allow_list, list)
	{
		if (filter->s_len > f_len)
			continue;
		if (filter->type == 0)
		{
			char *t = file + f_len - filter->s_len;
			if (strcmp(t, filter->strings))
				continue;
			ret = 0;
			break;
		}
	}
	return ret;
}

/*
 * return 1:deny 0:allow
 */
int check_file_filter(char *file)
{
	return check_file_allow(file);
}

/*
 * example:135320442046hd_qvga.mp4
 * return 1:deny 0:allow
 */
int check_mp4_filter(char *file)
{
	int ret = 1;
	int f_len = strlen(file);
	if(f_len > 4)
	{
		char *t = file + f_len - 4;
		if (!strcmp(t, ".mp4"))
		{
			ret = 0;
		}
	}
	return ret;
}

