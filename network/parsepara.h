/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

#ifndef _PARSEPARA_H
#define _PARSEPARA_H

#include <assert.h>

/*
    MAX_NAME_LEN    是参数名称的最大长度
    MAX_VALUE_LEN    是参数值得最大长度
*/
#define MAX_NAME_LEN    200
#define MAX_VALUE_LEN    4*1024

typedef struct {
    char sFirst[MAX_NAME_LEN];
    char sSecond[MAX_VALUE_LEN];
}StringPair;

typedef struct {
    int iLast;
    int iMaxPairNum;
    StringPair *pStrPairList;
}StringPairList;

#ifdef __cplusplus
extern "C" {
#endif

StringPairList *CreateStringPairList(int iListNum);
StringPairList *ConcatPairList(StringPairList *p1, const StringPairList *p2);


void DestroyStringPairList(StringPairList *pList);

void ResetStringPairList(StringPairList *pList);


void TraverseList(
    const StringPairList *pList, 
    int (*ProcessNameVal )(const char *, const char *, void *),
    void *pThis
    );


const char * GetParaValue(const StringPairList *pPairList, 
    const char *sName,
    char *sValue,
    size_t n);

int    SetParaValue(StringPairList *pPairList,
    const char *sName,
    const char *sValue);

int EncodePara(const StringPairList *pPairList, 
    char *sData,
    size_t *pDataLen);

int DecodePara(const char* sRequest,
    int iReqLen,
    StringPairList *pPairList);

int parsepara(const char *sParaStr,             /* 字符串*/
    int    iStrLen,                                /* 长度*/
    StringPair *pParaArr,                         /* 名-值数组                */
    int iPairLen,                                 /*名-值数组的长度*/
    char ch1,                                 /* 名和值之间的分割符号*/
    char ch2);                                /*名-值之间的分割符号*/

const char *GetBinaryPara(
    const StringPairList *pPairList,
    const char *sName,
    char *sBinaryVal,
    size_t *BinaryLength
    );
int SetBinaryPara(
    StringPairList *pPairList,
    const char *sName,
    const char *sBinaryVal,
    size_t length
    );

/*

    int LoadFromFile(const char *sMsgFile, StringPair *pPairArr, const int iMaxNum);

*/

#ifdef __cplusplus
}
#endif

#endif /*_PARSEPARA_H*/
