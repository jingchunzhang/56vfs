#ifndef _56CDC_HASH_H_
#define _56CDC_HASH_H_
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include "cdc_shm.h"

#define CDC_SHM_ID 0x1234abcd

enum {CDC_F_UNKNOWN = 0, CDC_F_OK, CDC_F_DEL, CDC_F_SYNCING};
extern char *s_ip_status[]; 
#define MAX_IP_IN_DIR 32

typedef struct
{
	char *base;
	size_t   shmsize;
	uint32_t datasize;
	uint32_t hashsize;
	uint32_t usedsize;
}t_cdc_shmhead;

typedef struct
{
	uint32_t hash1;
	uint32_t hash2;
	uint32_t hash3;
}t_cdc_key;

typedef struct
{
	char     fmd5[32];
	uint32_t fmtime;   /*file mtime*/
	uint32_t frtime;   /*file request time*/
	uint32_t fctime;   /*file ctime*/
	uint32_t ip[MAX_IP_IN_DIR];
	uint32_t mtime[MAX_IP_IN_DIR];
	uint64_t status_bists;
}t_cdc_val;

typedef struct
{
	t_cdc_key k;
	t_cdc_val v;
	uint32_t next;
}t_cdc_data;

int find_cdc_node(char *text, t_cdc_data ** data);

int add_cdc_node(char *text, t_cdc_val *v);

int add_cdc_node_by_key(t_cdc_key *k, t_cdc_val *v);

int cdc_sync_data(char *file);

int cdc_restore_data(char *file);

int link_cdc_read();

int link_cdc_write();

int link_cdc_create(size_t size);

int get_shm_baseinfo(t_cdc_shmhead **head);

int get_index_node(uint32_t index, t_cdc_data ** data);

int clear_index_node(uint32_t index);

#endif

