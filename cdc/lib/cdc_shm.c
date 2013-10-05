/*
* Copyright (C) 2012-2014 jingchun.zhang email: jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
* Please visit the github https://github.com/jingchunzhang/56vfs for more detail.
*/

#include "cdc_shm.h"

void *getshmadd(key_t id, size_t isize, int mode)
{
	void *pmem;

	int shmid = shmget(id, isize, isize > 0 ? (IPC_CREAT | mode|SHARE_SHM_PERM) : mode);
	if (shmid == -1) 
	{
		return NULL;
	}
	pmem = shmat(shmid, NULL, (mode & SHM_W) ? 0 : SHM_RDONLY);
	if (pmem == NULL || pmem == (void *) (-1)) 
	{
		return NULL;
	}
	return pmem;
}
