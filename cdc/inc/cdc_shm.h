#ifndef _56CDC_SHM_H
#define _56CDC_SHM_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>

#define SHARE_SHM_PERM 0644

void *getshmadd(key_t key, size_t size, int mode);

#endif

