#include "cdc_hash.h"
#include "GeneralHashFunctions.h"
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_ONCE_DISK 134217728   /*单次磁盘操作最大BUF*/

static void trim_sep(char *s, char sep)
{
	char buf[1024] = {0x0};
	int l = snprintf(buf, sizeof(buf), "%s", s);
	if (l >= 1023)
		return;
	memset(s, 0, l);
	char *t = buf;
	char *last = NULL;;
	while (*t)
	{
		if (last && *last == *t && *t == sep)
		{
			t++;
			continue;
		}
		*s = *t;
		if (*t == sep)
			last = t;
		else
			last = 0x0;
		s++;
		t++;
	}
}

static t_cdc_shmhead *cdc_head = NULL;

static t_cdc_data * cdc_base = NULL;

char *s_ip_status[] = {"CDC_F_UNKNOWN", "CDC_F_OK", "CDC_F_DEL", "CDC_F_SYNCING"};

static int init_cdc_hash(key_t key, size_t size, int mode)
{
	size_t bigsize = size * sizeof(t_cdc_data) + sizeof(t_cdc_shmhead);
	if (size == 0)
		bigsize = 0;
	cdc_head = (t_cdc_shmhead *)getshmadd(key, bigsize, mode);
	if (cdc_head == NULL)
		return -1;
	cdc_base = (t_cdc_data * ) ((char *) cdc_head + sizeof(t_cdc_shmhead));
	if (size == 0)
		return 0;

	memset(cdc_head, 0, bigsize);
	cdc_head->shmsize = bigsize;
	cdc_head->hashsize = size /4;
	cdc_head->datasize = size - cdc_head->hashsize;
	cdc_head->usedsize = 0;

	return 0;
}

int find_cdc_node(char *text, t_cdc_data ** data0)
{
	trim_sep(text, '/');
	unsigned int hash1, hash2, hash3;
	get_3_hash(text, &hash1, &hash2, &hash3);
	unsigned int hash = hash1%cdc_head->hashsize;

	t_cdc_data * data = cdc_base + hash;
	while (1)
	{
		t_cdc_key *k = &(data->k);
		if (k->hash1 == hash1 && k->hash2 == hash2 && k->hash3 == hash3)
		{
			*data0 = data;
			data->v.frtime = time(NULL);
			return 0;
		}
		if (data->next == 0)
			break;
		data = cdc_base + data->next;
	}
	return -1;
}

int add_cdc_node(char *text, t_cdc_val *v)
{
	trim_sep(text, '/');
	unsigned int hash1, hash2, hash3;
	get_3_hash(text, &hash1, &hash2, &hash3);
	t_cdc_key k;
	k.hash1 = hash1;
	k.hash2 = hash2;
	k.hash3 = hash3;
	return add_cdc_node_by_key(&k, v);
}

int add_cdc_node_by_key(t_cdc_key *k0, t_cdc_val *v)
{
	unsigned int hash = k0->hash1%cdc_head->hashsize;

	t_cdc_data * data = cdc_base + hash;
	t_cdc_data * newone = NULL;
	t_cdc_key *k = &(data->k);
	if (k->hash1 || k->hash2 || k->hash3 )
	{
		while (data->next)
		{
			k = &(data->k);
			if (k->hash1 == 0 && k->hash2 == 0 && k->hash3 == 0)
			{
				newone = data;
				break;
			}
			data = cdc_base + data->next;
		}
		if (newone == NULL)
		{
			if (cdc_head->usedsize >= cdc_head->datasize)
				return -1;
			newone = cdc_base + cdc_head->hashsize + cdc_head->usedsize;
			data->next = cdc_head->hashsize + cdc_head->usedsize;
			cdc_head->usedsize++;
		}
	}
	else
		newone = data;
	memcpy(&(newone->k), k0, sizeof(t_cdc_key));
	memcpy(&(newone->v), v, sizeof(t_cdc_val));
	return 0;
}

int cdc_sync_data(char *file)
{
	int fd;
	if ((fd = open64(file, O_CREAT | O_RDWR | O_LARGEFILE, 0644)) < 0)
		return -1;

	fcntl(fd, F_SETFL, O_RDWR|O_NONBLOCK);
	char *s = (char *)cdc_head;
	size_t retlen = cdc_head->shmsize;
	size_t wlen = 0;
	size_t once = 0;
	while (retlen > 0)
	{
		once = retlen <= MAX_ONCE_DISK ? retlen : MAX_ONCE_DISK;
		wlen = write(fd, s, once);
		if (wlen <= 0)
		{
			close(fd);
			return -1;
		}
		s += wlen;
		retlen -= wlen;
		fsync(fd);
	}

	close(fd);
	return 0;
}

int cdc_restore_data(char *file)
{
	struct stat64 filestat;
	if (stat64(file, &filestat) < 0)
		return -1;

	cdc_head = (t_cdc_shmhead *)getshmadd(CDC_SHM_ID, filestat.st_size, SHARE_SHM_PERM);
	if (cdc_head == NULL)
		return -1;
	cdc_base = (t_cdc_data * ) ((char *) cdc_head + sizeof(t_cdc_shmhead));
	int fd;
	if ((fd = open64(file, O_RDONLY)) < 0)
		return -1;

	char *s = (char *)cdc_head;
	size_t retlen = filestat.st_size;
	size_t rlen = 0;
	size_t once = 0;
	while (retlen > 0)
	{
		once = retlen <= MAX_ONCE_DISK ? retlen : MAX_ONCE_DISK;
		rlen = read(fd, s, once);
		if (rlen <= 0)
		{
			close(fd);
			return -1;
		}
		s += rlen;
		retlen -= rlen;
	}

	close(fd);
	return 0;
}

int link_cdc_read()
{
	return init_cdc_hash(CDC_SHM_ID, 0, 0444);
}

int link_cdc_write()
{
	return init_cdc_hash(CDC_SHM_ID, 0, SHARE_SHM_PERM);
}

int link_cdc_create(size_t size)
{
	return init_cdc_hash(CDC_SHM_ID, size, SHARE_SHM_PERM);
}

int get_shm_baseinfo(t_cdc_shmhead **head)
{
	*head = cdc_head;
	return 0;
}

int get_index_node(uint32_t index, t_cdc_data ** data)
{
	if (index >= cdc_head->hashsize + cdc_head->datasize)
		return -1;
	*data = cdc_base + index;
	return 0;
}

int clear_index_node(uint32_t index)
{
	t_cdc_data *data;
	if (index >= cdc_head->hashsize + cdc_head->datasize)
		return -1;
	data = cdc_base + index;
	t_cdc_key *k = &(data->k);
	memset(k, 0, sizeof(t_cdc_key));
	return 0;
}


