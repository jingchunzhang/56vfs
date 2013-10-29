
#ifndef TSHARE_MEM_H
#define TSHARE_MEM_H

#include <stdio.h>
#include <stdlib.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/ipc.h>

using namespace std;

#define SHARE_SHM_PERM 0666

class TShareMem {
private:
	int m_shmid;
	void *m_pmem;

public:
	TShareMem();
	
	void *f_getMem(key_t id, size_t isize, int mode = SHARE_SHM_PERM);
	
	void f_delMem();

	void f_unlinkMem();

	size_t f_getSize();

	int f_getShmID();
};

#endif
