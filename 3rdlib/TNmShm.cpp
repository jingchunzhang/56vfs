#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "TShmBuffer.h"
#include "TNmShm.h"
#include "TShareMem.h"

#define MIN_PAGESIZE 4096

NMSHM::NMSHM()
{
	init_ok = -1;
}

NMSHM::~NMSHM()
{
}

int NMSHM::init_shm(int index, key_t intkey, key_t strkey)
{
	int ret = m_shmmap_int[index].Init(intkey);
	if (ret != 0)
	{
		ret = m_shmmap_int[index].Alloc(intkey, SHM_INT_SIZE/4, SHM_INT_SIZE);
		if (ret)
		{
			fprintf(stderr, "create shm error %m\n");
			return -1;
		}
	}

	ret = m_shmmap_str[index].Init(strkey);
	if (ret != 0)
	{
		ret = m_shmmap_str[index].Alloc(strkey, SHM_STR_SIZE/4, SHM_STR_SIZE);
		if (ret)
		{
			fprintf(stderr, "create shm error %m\n");
			return -1;
		}
	}
	return 0;
}

int NMSHM::init_int_str_shm()
{
	if (init_shm(0, SHM_INT_ID_1, SHM_STR_ID_1))
		return -1;
	if (init_shm(1, SHM_INT_ID_2, SHM_STR_ID_2))
		return -1;
	init_ok = 0;
	return 0;
}

int NMSHM::link_shm()
{
	//TShareMem control;
	void *addr = control.f_getMem(SHM_CONTROL_ID, 0, SHARE_SHM_PERM);
	if (addr == NULL)
	{
		addr = control.f_getMem(SHM_CONTROL_ID, SHM_CONTROL_BYTE, SHARE_SHM_PERM);
		if (addr == NULL)
		{
			fprintf(stderr, "create shm error %s\n", strerror(errno));
			return -1;
		}
		memset(addr, 0, SHM_CONTROL_BYTE);
	}
	cshm = (control_shm *)addr;
	return init_int_str_shm();
}

int NMSHM::begin_read_shm()
{
	unsigned char curindex = cshm->index;
	cshm->index = ~curindex;
	TShmBuffer<MNM_INT> * datashm = &m_shmmap_int[curindex%2];
	datashm->Begin();
	return curindex;
}

int NMSHM::end_read_shm(unsigned char index)
{
	TShmBuffer<MNM_INT> * datashm = &m_shmmap_int[index%2];
	datashm->Clear();
	return 0;
}

int NMSHM::read_shm(unsigned char index, int *alarmid, MNM_INT **val)
{
	TShmBuffer<MNM_INT> * datashm = &m_shmmap_int[index%2];

	while (1)
	{
		if (datashm->Next(alarmid, *val))
		{
			return -1;
		}
		return 0;
	}
	return 0;
}

int NMSHM::set_shm(int alarmid, MNM_INT *val)
{
	unsigned char curindex = cshm->index;
	TShmBuffer<MNM_INT> * datashm = &m_shmmap_int[curindex%2];

	MNM_INT *tv;
	if (datashm->Find(alarmid, tv) == 0)
	{
		memcpy(tv, val, sizeof(MNM_INT));
		return 0;
	}

	if (datashm->Add(alarmid, val) == 0)
	{
		return 0;
	}
	return -1;
}

int NMSHM::begin_read_shm_str()
{
	unsigned char curindex = cshm->strindex;
	cshm->strindex = ~curindex;
	TShmBuffer<MNM_STR> * datashm = &m_shmmap_str[curindex%2];
	datashm->Begin();
	return curindex;
}

int NMSHM::end_read_shm_str(unsigned char index)
{
	TShmBuffer<MNM_STR> * datashm = &m_shmmap_str[index%2];
	datashm->Clear();
	return 0;
}

int NMSHM::read_shm_str(unsigned char index, int *alarmid, MNM_STR **val)
{
	TShmBuffer<MNM_STR> * datashm = &m_shmmap_str[index%2];

	while (1)
	{
		if (datashm->Next(alarmid, *val))
		{
			return -1;
		}
		return 0;
	}
	return 0;
}

int NMSHM::set_shm_str(int alarmid, MNM_STR *val)
{
	unsigned char curindex = cshm->strindex;
	TShmBuffer<MNM_STR> * datashm = &m_shmmap_str[curindex%2];

	MNM_STR *tv;
	if (datashm->Find(alarmid, tv) == 0)
	{
		memcpy(tv, val, sizeof(MNM_STR));
		return 0;
	}

	if (datashm->Add(alarmid, val) == 0)
	{
		return 0;
	}
	return -1;
}

int NMSHM::inc_shm(int alarmid, MNM_INT *val)
{
	unsigned char curindex = cshm->index;
	TShmBuffer<MNM_INT> * datashm = &m_shmmap_int[curindex%2];
	MNM_INT *tv;
	if (datashm->Find(alarmid, tv) == 0)
	{
		tv->value += val->value;
		return 0;
	}

	if (datashm->Add(alarmid, val) == 0)
	{
		return 0;
	}
	return -1;
}

int NMSHM::get_init_flag()
{
	return init_ok;
}

NMSHM * uniNmShm(void)
{
	static NMSHM nmshm;
	return &nmshm;
}


int NMSHM::check_timestamp()
{
    int ret = 0;
    TShmBuffer<MNM_INT> * intshm = &m_shmmap_int[0];
    if (intshm->Alive() == 0/*timeout*/){
        intshm->Disconnect();
        ret = -1;
    }
    intshm = &m_shmmap_int[1];
    if (intshm->Alive() == 0){
        intshm->Disconnect();
        ret = -1;
    }

    TShmBuffer<MNM_STR> *strshm = &m_shmmap_str[0];
    if (strshm->Alive() == 0){
        strshm->Disconnect();
        ret = -1;
    }
    strshm = &m_shmmap_str[1];
    if (strshm->Alive() == 0){
        strshm->Disconnect();
        ret = -1;
    }
    return ret;
}


void NMSHM::update_last()
{
    m_shmmap_int[0].KeepAlive();
    m_shmmap_int[1].KeepAlive();
    
    m_shmmap_str[0].KeepAlive();
    m_shmmap_str[1].KeepAlive();
}
