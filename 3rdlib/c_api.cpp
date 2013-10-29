#include "TNmShm.h"
#include "c_api.h"
void SetInt(uint32_t key, uint32_t val)
{
	NMSHM * nmshm = uniNmShm();
	if (nmshm->get_init_flag() || nmshm->check_timestamp())
	{
		if (nmshm->link_shm())
			return;
	}
	MNM_INT mnm_int;
	mnm_int.value = val;
	nmshm->set_shm(key, &mnm_int);
}

void IncInt(uint32_t key, uint32_t val)
{
	NMSHM * nmshm = uniNmShm();
	if (nmshm->get_init_flag() || nmshm->check_timestamp())
	{
		if (nmshm->link_shm())
			return;
	}
	MNM_INT mnm_int;
	mnm_int.value = val;
	nmshm->inc_shm(key, &mnm_int);
}

void SetStr(uint32_t key, char *val)
{
	NMSHM * nmshm = uniNmShm();
	if (nmshm->get_init_flag() || nmshm->check_timestamp())
	{
		if (nmshm->link_shm())
			return;
	}
	MNM_STR mnm_str;
	int l = snprintf(mnm_str.buf, sizeof(mnm_str.buf) - 1, "%s", val);
	mnm_str.buf[l] = 0x0;
	nmshm->set_shm_str(key, &mnm_str);
}


void touch_timestamp()
{
    NMSHM *nmshm = uniNmShm();
    if (nmshm->get_init_flag())
    {
        if (nmshm->link_shm())
            return ;
    }
    nmshm->update_last();
}

