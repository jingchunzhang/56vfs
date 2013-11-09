#ifndef __NM_APP_VFS_H__
#define __NM_APP_VFS_H__

#include "c_api.h"

#define NM_INT_VFS_BASE 0x04001000
#define NM_INC_VFS_BASE 0x05001000
#define NM_STR_VFS_BASE 0x08001000

#define VFS_STR_OPENFILE_E NM_STR_VFS_BASE+1   /*打开文件错误*/
#define VFS_STR_CONNECT_E NM_STR_VFS_BASE+2   /*连接对端错误*/

#define VFS_STR_MD5_E  NM_STR_VFS_BASE+3    /*MD5校验错误*/
#define VFS_WRITE_FILE NM_STR_VFS_BASE+4    /**/
#define VFS_MALLOC NM_STR_VFS_BASE+5    /**/
#define VFS_TASK_COUNT NM_STR_VFS_BASE+6    /**/
#define VFS_TASK_MUTEX_ERR NM_STR_VFS_BASE+7  /*用于框架同步报错*/
#define VFS_START_ERR NM_STR_VFS_BASE+8  /*VFS Start ERROR*/
#define VFS_UNTRUST_IP NM_STR_VFS_BASE+9  /*VOSS untrust ip try push file*/
#define VFS_GET_IP_ERR NM_STR_VFS_BASE+10  /*cs get ip err*/

#define VFS_INT_TASK_ALL NM_INT_VFS_BASE+1  /*当前机器所有任务数*/
#define VFS_INC_TASK_OVER NM_INT_VFS_BASE+2 /*已完成任务数*/
#define VFS_TASK_COUNT_INT NM_INT_VFS_BASE+3    /**/

#define VFS_TASK_DEPTH_BASE NM_INT_VFS_BASE+10  /*TASK_DEPTH_BASE max 32 queue*/

#define VFS_RE_EXECUTE_INC NM_INC_VFS_BASE
#define VFS_ABORT_INC NM_INC_VFS_BASE+1

/*
 * CDC
 */
#define NM_STR_CDC_BASE NM_STR_VFS_BASE+1000
#define CDC_NORMAL_ERR NM_STR_CDC_BASE /*CDC open file , link file , open dir etc error*/
#define CDC_TOO_MANY_IP NM_STR_CDC_BASE+1 /*CDC too many ip one file*/
#define CDC_ADD_NODE_ERR NM_STR_CDC_BASE+2  /*CDC add node err*/
#define CDC_SHM_INIT_ERR NM_STR_CDC_BASE+3  /*CDC shm init err*/

#endif
