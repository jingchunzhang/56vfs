/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#ifndef __56NM_PLUGINS_H__
#define __56NM_PLUGINS_H__

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include "list.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef int (*so_method) ();

typedef struct {
	void* handle;
	so_method	so_init;
	so_method	so_proc;
	so_method   so_fini;
	list_head_t list;
} silib;

int init_libs();
void fini_libs();
void scan_libs();

#ifdef __cplusplus
}
#endif
#endif
