#include "TShareMem.h"

TShareMem::TShareMem()
{
	m_pmem = NULL;
	m_shmid = -1;
}

void *TShareMem::f_getMem(key_t id, size_t isize, int mode)
{
	void *pmem;
	if (m_pmem != NULL)
		return m_pmem;

	key_t key = id;
	m_shmid = shmget(key, isize, isize > 0 ? (IPC_CREAT | mode) : mode);
	if (m_shmid == -1) {
		return NULL;
	}
	pmem = shmat(m_shmid, NULL, (mode & SHM_W) ? 0 : SHM_RDONLY);
	if (pmem == NULL || pmem == (void *) (-1)) {
		return NULL;
	}
	m_pmem = pmem;
	return pmem;
}

void TShareMem::f_unlinkMem()
{
	if (m_pmem != NULL) {
#ifdef sunos
		shmdt((char *)m_pmem);
#else
		shmdt(m_pmem);
#endif
		m_pmem = NULL;
		m_shmid = -1;
	}
}

void TShareMem::f_delMem()
{
	if (m_pmem != NULL) {
#ifdef sunos
		shmdt((char *)m_pmem);
#else
		shmdt(m_pmem);
#endif
		shmctl(m_shmid, IPC_RMID, 0);
		m_pmem = NULL;
		m_shmid = -1;
	}
}

size_t TShareMem::f_getSize()
{
	struct shmid_ds shmbuf;

	if (shmctl(m_shmid, IPC_STAT, &shmbuf) == -1) {
		return (-1);
	}
	return (shmbuf.shm_segsz);
}

int TShareMem::f_getShmID()
{
	return m_shmid;
}

