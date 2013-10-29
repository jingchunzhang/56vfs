#ifndef __TNmShm_H_
#define __TNmShm_H_

#include "TShmBuffer.h"
#include "TShareMem.h"
#include "c_api.h"

typedef struct
{
	unsigned char index;
	unsigned char strindex;
	unsigned char bk[2];
} control_shm;

typedef struct
{
	int value;
}MNM_INT;

typedef struct
{
	char buf[252];
}MNM_STR;

enum {APP = 0, AGENT};

#ifdef _TEST_DOG_
#define SHM_CONTROL_ID  0xF0001234
#define SHM_INT_ID_1    0x0F000001
#define SHM_INT_ID_2    0x0F000002
#define SHM_STR_ID_1    0x0F000003
#define SHM_STR_ID_2    0x0F000004
#else
#define SHM_CONTROL_ID 0x80001234
#define SHM_INT_ID_1 0x08000001
#define SHM_INT_ID_2 0x08000002

#define SHM_STR_ID_1 0x08000003
#define SHM_STR_ID_2 0x08000004
#endif


#define SHM_INT_SIZE 81920     //假设有8192的数值型告警ID
#define SHM_STR_SIZE 2560	//假设一台机器有256字符串型告警ID

#define SHM_CONTROL_BYTE 4   //物理大小

class NMSHM
{
	public:
		NMSHM();

		~NMSHM();

	public:

		int link_shm();
        
        int check_timestamp();
        
        void update_last();

	public:

		int begin_read_shm();

		int read_shm(unsigned char index, int *alarmid, MNM_INT **val);

		int end_read_shm(unsigned char index);

		int set_shm(int keyno, MNM_INT *val);

		int inc_shm(int keyno, MNM_INT *val);

	public:

		int begin_read_shm_str();

		int read_shm_str(unsigned char index, int *alarmid, MNM_STR **val);

		int end_read_shm_str(unsigned char index);

		int set_shm_str(int keyno, MNM_STR *val);

	public:
		int get_init_flag();

	protected:

		int init_int_str_shm();

		int init_shm(int index, key_t key1, key_t key2);

	private:

		TShmBuffer<MNM_INT> m_shmmap_int[2];

		TShmBuffer<MNM_STR> m_shmmap_str[2];

	    TShareMem control;
		control_shm * cshm;

		int init_ok;
};

#ifdef __cplusplus
extern "C"
{
#endif
NMSHM * uniNmShm(void);
#ifdef __cplusplus
}
#endif

#endif
