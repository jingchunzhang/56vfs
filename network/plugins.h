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
