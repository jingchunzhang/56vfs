/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#include <dlfcn.h>
#include "plugins.h"
#include "myconfig.h"

list_head_t plist;

void scan_libs()
{
	silib *lib = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(lib, l, &(plist), list)
	{
		lib->so_proc();
	}
}

int init_libs()
{
	INIT_LIST_HEAD(&plist);
	char* pval = NULL; int i = 0;
	for( i = 0; ( pval = myconfig_get_multivalue( "plugin_so", i ) ) != NULL; i++ )
	{
		silib * lpplugin = (silib*)malloc( sizeof(silib) );
		if( lpplugin == NULL ) 
		{
			fprintf(stderr, "%s %d %m\n", __FILE__, __LINE__);
			return -1;
		}
		lpplugin->handle = dlopen( pval, RTLD_NOW);
		if( lpplugin->handle == NULL )
		{
			fprintf(stderr, "%s %d %m [%s] %s\n", __FILE__, __LINE__, pval, dlerror());
			return -1;
		}
		lpplugin->so_init = (so_method)dlsym( lpplugin->handle, "so_init" );
		lpplugin->so_proc = (so_method)dlsym( lpplugin->handle, "so_proc" );
		if (lpplugin->so_proc == NULL)
		{
			fprintf( stderr, "Plugin %s no so_proc.", pval );
			return -1;
		}
		lpplugin->so_fini = (so_method)dlsym( lpplugin->handle, "so_fini" );
		if( lpplugin->so_init != NULL )
		{
			if( lpplugin->so_init() != 0 )
			{
				fprintf( stderr, "Plugin %s so_init fail.", pval );
				return -1;
			}
		}
		INIT_LIST_HEAD(&(lpplugin->list));
		list_add_(&(lpplugin->list), &plist);
	}
	return 0;
}

void fini_libs()
{
	silib *lib = NULL;
	list_head_t *l;
	list_for_each_entry_safe_l(lib, l, &(plist), list)
	{
		if (lib->so_fini)
			lib->so_fini();
		list_del_init(&(lib->list));
		free(lib);
	}
}

