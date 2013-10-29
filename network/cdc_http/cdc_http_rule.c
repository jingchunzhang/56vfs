/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include "list.h"
#include "log.h"
#include "global.h"
#include "vfs_init.h"
#include "util.h"
#include <stdio.h>
#include <stddef.h>

typedef struct {
	uint8_t isp[MAXISP]; 
	uint8_t index;
} t_valid_isp;

static t_valid_isp valid_isp[MAXISP];

static uint8_t get_index_isp(char *s)
{
	uint8_t i = 0;
	for (i = 0; i < MAXISP; i++)
	{
		if (strcasecmp(s, ispname[i]) == 0)
			break;
	}
	return i;
}

static void init_rule_sub(int i, char *v)
{
	t_valid_isp *isp = &(valid_isp[i%MAXISP]);
	memset(isp, 0, sizeof(t_valid_isp));

	LOG(vfs_http_log, LOG_NORMAL, "%s %d %s:%s\n", ID, LN, ispname[i%MAXISP], v);
	uint8_t sisp = MAXISP;
	char *s = v;
	while (1)
	{
		char *t = strchr(s, ',');
		if (t == NULL)
			break;
		*t = 0x0;
		LOG(vfs_http_log, LOG_NORMAL, "%s %d %s:%s\n", ID, LN, ispname[i%MAXISP], s);
		sisp = get_index_isp(s);
		*t = ',';
		s = t + 1;
		if (sisp < MAXISP)
		{
			isp->isp[isp->index] = sisp;
			isp->index++;
			LOG(vfs_http_log, LOG_NORMAL, "%s %d %s:%s:%d\n", ID, LN, ispname[i%MAXISP], ispname[sisp], isp->index);
		}
		else
			LOG(vfs_http_log, LOG_ERROR, "err config %s:%s\n", ispname[i], v);
	}
	sisp = get_index_isp(s);
	if (sisp < MAXISP)
	{
		isp->isp[isp->index] = sisp;
		isp->index++;
		LOG(vfs_http_log, LOG_NORMAL, "%s %d %s:%s:%d\n", ID, LN, ispname[i%MAXISP], ispname[sisp], isp->index);
	}
	else
		LOG(vfs_http_log, LOG_ERROR, "err config %s:%s\n", ispname[i], v);
}

static void re_adjust_rule(uint8_t i)
{
	uint8_t j = i%MAXISP;
	t_valid_isp *isp = &(valid_isp[j]);
	if (j == YD)
	{
		isp->index = 2;
		isp->isp[0] = CNC;
		isp->isp[1] = TEL;
	}
	else if (j == TT)
	{
		isp->index = 2;
		isp->isp[0] = YD;
		isp->isp[1] = CNC;
	}
	else 
	{
		isp->index = 1;
		isp->isp[0] = j;
	}
}

static void scan_cdc_rule()
{
	uint8_t i = 0;
	for (i = 0; i < MAXISP; i++)
	{
		t_valid_isp *isp = &(valid_isp[i]);
		char v[128] = {0x0};
		char *t = v;
		uint8_t j = 0;
		for (j = 0; j < isp->index; j++)
		{
			t += sprintf(t, "%s,", ispname[isp->isp[j]]);
		}
		LOG(vfs_http_log, LOG_DEBUG, "%s config %s:%s\n", ID, ispname[i], v);
		if(isp->index == 0)
			re_adjust_rule(i);
	}
}

static int init_cdc_rule()
{
	memset(valid_isp, 0, sizeof(valid_isp));
	char k[128] = {0x0};
	uint8_t i = 0;
	for (i = 0; i < MAXISP; i++)
	{
		memset(k, 0, sizeof(k));
		snprintf(k, sizeof(k), "rule_%s", ispname[i]);
		char *v = myconfig_get_value(k);
		if (v == NULL)
			continue;
		init_rule_sub(i, v);
	}
	scan_cdc_rule();
	return 0;
}
