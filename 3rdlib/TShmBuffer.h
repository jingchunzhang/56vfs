#ifndef _TSHMBUFFER_H
#define _TSHMBUFFER_H

#include "TShareMem.h"
#include "TException.h"

#include <errno.h>
#include <memory.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#define TMP_SHM_KEY 0x2000abcd
#define TIMEOUT 300

struct SHM_HEAD
{
	time_t timestamp;
	int hash_size;
	int data_size;
	int used_size;
	int total_size;
	long buf_size;
};

template <typename T>
class SHM_DATA_STRU
{
	public:
		unsigned int keyno;
		T value;
		unsigned int next;
};

template <typename T>
class TShmBuffer
{
	public:

		TShmBuffer();

		~TShmBuffer();

		int Init(const key_t &key, const int &mode = SHARE_SHM_PERM);

		int Alloc(const key_t &key, int nhash = 0, int nrec = 0);

		void Begin(void);

		int Erase(int keyno);

		int Next(int *keyno, T *&result);

		int Next(const int &keyno, T *&result);

		int Add(const int &keyno, const T *data);

		int Find(const int &keyno, T *&result);

		void Clear(void);

		void Destroy(void);

		int Disconnect(void);

		int Size(void);

		int Delete(SHM_DATA_STRU<T> *node);

		int UnitSize(void);

		key_t GetKey(void);

		bool Alive(void);

        void KeepAlive(void);

		int Bufsize(void);

		unsigned int GetShmId();

	protected:

		int Realloc(int nhash = 0, int nrec = 0);

	private:

		SHM_HEAD *m_shmHead;

		SHM_DATA_STRU<T> *m_pNode, *m_buffer;

		TShareMem m_shareMem;

		key_t m_key;
};

template <typename T> 
TShmBuffer<T>::TShmBuffer()
{
	m_shmHead = NULL;
	m_pNode = m_buffer = NULL;
}

template <typename T> 
TShmBuffer<T>::~TShmBuffer()
{
}

template <typename T> 
int TShmBuffer<T>::Init(const key_t &key, const int &mode)
{
	m_key = key;

	void *buf = m_shareMem.f_getMem(m_key, 0, mode);
	if (buf == NULL || buf == (void*)-1)
		return -1;
	m_shmHead = (SHM_HEAD*)buf;
	m_buffer = (SHM_DATA_STRU<T>*)((char*)buf + sizeof(SHM_HEAD));

	return 0;
}

template <typename T> 
int TShmBuffer<T>::Alloc(const key_t &key, int nhash, int nrec)
{
	m_key = key;

	int buf_size = sizeof(SHM_HEAD) + sizeof(SHM_DATA_STRU<T>) * nrec;
	void *buf = m_shareMem.f_getMem(m_key, buf_size);
	if (buf == NULL)
	{
		fprintf(stderr, "m_shareMem.f_getMem k[%x] size [%d] err %m\n", m_key, buf_size);
		return -1;
	}

	memset(buf, 0, buf_size);
	m_shmHead = (SHM_HEAD*)buf;
	m_shmHead->buf_size = buf_size;
	m_shmHead->hash_size = nhash;
	m_shmHead->data_size = nrec - nhash;
	m_shmHead->used_size = 0;
	m_shmHead->total_size = 0;
	m_shmHead->timestamp= time(NULL);

	m_buffer = (SHM_DATA_STRU<T>*)((char*)buf + sizeof(SHM_HEAD));

	return 0;
}

template <typename T> 
int TShmBuffer<T>::Add(const int &keyno, const T *data)
{
	int hash = keyno % m_shmHead->hash_size;
	SHM_DATA_STRU<T> *last = m_buffer + hash;
	SHM_DATA_STRU<T> *newone = NULL;

	if (last->keyno > 0)
	{
		while (true)
		{
			if (last->keyno == 0)
			{
				newone = last;
				break;
			}

			if (last->next == 0)
				break;

			last = m_buffer + last->next;
		}

		if (newone == NULL)
		{
			if (m_shmHead->used_size >= m_shmHead->data_size)
			{
				if (Realloc() < 0)
					return -1;
				return Add(keyno, data);
			}

			last->next = m_shmHead->hash_size + m_shmHead->used_size;
			newone = m_buffer + last->next;
			m_shmHead->used_size += 1;
		}
	}
	else
	{
		newone = last;
	}

	newone->keyno = keyno;
	memcpy(&(newone->value), data, sizeof(T));

	m_shmHead->total_size += 1;

	return 0;
}

template <typename T> 
int TShmBuffer<T>::Find(const int &keyno, T *&result)
{
	int hash = keyno % m_shmHead->hash_size;
	SHM_DATA_STRU<T> *p = m_buffer + hash;

	while (true)
	{
		if (p->keyno == keyno)
		{
			result = &(p->value);
			//m_pNode = p;
			return 0;
		}

		if (p->next == 0)
			break;

		p = m_buffer + p->next;
	}

	result = NULL;
//	m_pNode = NULL;

	return -1;
}

template <typename T> 
int TShmBuffer<T>::Erase(int keyno)
{
	int hash = keyno % m_shmHead->hash_size;
	SHM_DATA_STRU<T> *p = m_buffer + hash;

	while (true)
	{
		if (p->keyno == keyno)
		{
			p->keyno = 0;
			return 0;
		}

		if (p->next == 0)
			break;

		p = m_buffer + p->next;
	}

	return -1;
}

template <typename T> 
void TShmBuffer<T>::Clear(void)
{
	memset(m_buffer, 0, sizeof(SHM_DATA_STRU<T>) * (m_shmHead->hash_size + m_shmHead->data_size));
	m_shmHead->used_size = m_shmHead->total_size = 0;
}

template <typename T> 
bool TShmBuffer<T>::Alive(void)
{
    time_t curr = time(NULL);
    if (curr - m_shmHead->timestamp > TIMEOUT){
        return false;
    }
    return true;
}

template <typename T> 
void TShmBuffer<T>::KeepAlive(void)
{
    m_shmHead->timestamp = time(NULL);
}

template <typename T> 
void TShmBuffer<T>::Begin()
{
	m_pNode = NULL;
}

template <typename T> 
int TShmBuffer<T>::Next(int *keyno, T *&result)
{
	if (m_pNode == NULL)
		m_pNode = m_buffer;

	while(m_pNode < m_buffer + m_shmHead->hash_size + m_shmHead->used_size)
	{
		if (m_pNode->keyno > 0)
		{
			result = &(m_pNode->value);
			*keyno = m_pNode->keyno;
			++m_pNode;
			return 0;
		}

		++m_pNode;
	}

	return -1;
}

template <typename T> 
int TShmBuffer<T>::Next(const int &keyno, T *&result)
{
	if (m_pNode == NULL)
		return Find(keyno, result);

	SHM_DATA_STRU<T> *node = m_pNode;
	while(node->next > 0)
	{
		node = m_buffer + node->next;

		if (node->keyno == keyno)
		{
			m_pNode = node;
			result = &(node->value);
			return 0;
		}
	}

	m_pNode = NULL;
	return -1;
}

template <typename T> 
int TShmBuffer<T>::Realloc(int nhash, int nrec)
{
	TShareMem shmem;
	SHM_DATA_STRU<T> *p;

	if (nhash <= 0 || nrec <= 0 )
	{
		nhash = m_shmHead->hash_size * 12 / 10;
		nrec = (m_shmHead->hash_size + m_shmHead->data_size) * 12 / 10;
	}

	char *tmp;
	if ((tmp = (char*)shmem.f_getMem(TMP_SHM_KEY, 0)) != NULL)
		shmem.f_delMem();
	tmp = (char*)shmem.f_getMem(TMP_SHM_KEY, m_shmHead->buf_size);
	if (tmp == NULL)
		return -1;

	memcpy(tmp, (char*)m_shmHead, m_shmHead->buf_size);
	Destroy();

	if (Alloc(m_key, nhash, nrec) < 0)
	{
		shmem.f_delMem();
		return -1;
	}

	SHM_HEAD *head = (SHM_HEAD*)tmp;
	SHM_DATA_STRU<T> *buf = (SHM_DATA_STRU<T> *)((char*)tmp + sizeof(SHM_HEAD));
	SHM_DATA_STRU<T> *end = buf + head->hash_size + head->data_size;
	
	for (p = buf; p < end; ++p)
	{
		if (p->keyno > 0)
		{
			if (Add(p->keyno, &(p->value)) < 0)
			{
				shmem.f_delMem();
				return -1;
			}
		}
	}

	shmem.f_delMem();

	Disconnect();

	return Init(m_key);
}

template <typename T> 
void TShmBuffer<T>::Destroy()
{
	m_shareMem.f_delMem();
}

template <typename T> 
int TShmBuffer<T>::Disconnect()
{
	m_shareMem.f_unlinkMem();
	return 0;
}

template <typename T> 
int TShmBuffer<T>::Size(void)
{
	return m_shmHead->total_size;
}

template <typename T> 
int TShmBuffer<T>::Delete(SHM_DATA_STRU<T> *node)
{
	node->keyno = 0;
	m_shmHead->total_size -= 1;

	return 0;
}

template <typename T> 
int TShmBuffer<T>::UnitSize()
{
	return sizeof(SHM_DATA_STRU<T>);
}

template <typename T> 
int TShmBuffer<T>::Bufsize()
{
	return m_shmHead->buf_size;
}

template <typename T>
key_t TShmBuffer<T>::GetKey(void)
{
	return m_key;
}

template <typename T>
unsigned int TShmBuffer<T>::GetShmId(void)
{
	return m_shareMem.f_getShmID();
}

#endif
