/*
* Copyright (C) 2012-2014 www.56.com email: jingchun.zhang AT renren-inc.com; jczhang AT 126.com ; danezhang77 AT gmail.com
* 
* 56VFS may be copied only under the terms of the GNU General Public License V3
*/

/*
 * parsepara.c           Source file of parsing the input parameters.
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include "parsepara.h"

#define ENCODE_TABLE        "=&%"



inline int shouldencode(char ch)
{
    unsigned int i;

    for (i = 0; ENCODE_TABLE[i] != 0; i++)
    {
        if (ch == ENCODE_TABLE[i])
            return 1;
    }
    return 0;
}

int encode_hexstring(const char *s, int len, char *sEncoded)
{
    int i;
    char sHexTable[17] = "0123456789ABCDEF";

    for (i = 0; i < len; i++)
    {
        if (!isprint(s[i]) || shouldencode(s[i]))
        {
            *sEncoded = '%';
            sEncoded ++;
            *sEncoded = sHexTable[(s[i]>>4)&0x0f];
            sEncoded ++;
            *sEncoded = sHexTable[s[i]&0x0f];
            sEncoded++;
        }
        else
        {
            *sEncoded = s[i];
            sEncoded ++;
        }
    }
    *sEncoded = 0;
    return 0;
}

int encode_string(const char *s, char *sEncoded)
{
    unsigned int i;
    char sHexTable[17] = "0123456789ABCDEF";

    for (i = 0; s[i] != 0 ; i++)
    {
        if (shouldencode(s[i]))
        {
            *sEncoded = '%';
            sEncoded ++;
            *sEncoded = sHexTable[(s[i]>>4)&0x0f];
            sEncoded ++;
            *sEncoded = sHexTable[s[i]&0x0f];
            sEncoded++;
        }
        else
        {
            *sEncoded = s[i];
            sEncoded ++;
        }
    }
    *sEncoded = 0;
    return 0;
}

inline int get_index(char ch)
{
    int i;
    char sHexTable[17] = "0123456789ABCDEF";

    for (i = 0; i < 16; i++)
        if (sHexTable[i] == toupper(ch))
            return i;
    return -1;
}


int decode_hexstring(const char *s, char *sDecoded, int *piDecodedLen)
{
    unsigned int i = 0;
    char j, k;
    char *sStart = sDecoded;


   
    while (i < strlen(s))
    {
        if (s[i] == '%')
        {
            if (i >= strlen(s)-2)
                return -1;
            
            j = get_index(s[i+1]);
            k = get_index(s[i+2]);
            if (j < 0 || k < 0)
            {
                printf("%c %c\n", s[i+1], s[i+2]);
                return -2;
            }
            *sDecoded = (j << 4) +k;
            sDecoded ++;
            i += 3;
        }
        else
        {
            *sDecoded = s[i ++];
            sDecoded ++;
        }
    }
    *piDecodedLen = (sDecoded - sStart);
    return 0;
}

inline int decode_stringnew(const char *s, char *dst, size_t n)
{
    int i,j;
    while(*s != 0) {
        if (*s != '%')
            *dst++ = *s++;
        else {
            if (*(s+1) == 0)
                break;
            i = get_index(*(s+1));
            if (*(s+2) == 0)
                break;
            j = get_index(*(s+2));
            
            *dst++ = (i<<4)+j;
            s += 3;
        }
    }
    return 0;
}

inline int decode_string(const char *s, char *sDecoded, size_t n)
{
    unsigned int i = 0;
    char j, k;
    char *bufend = sDecoded+n-1;
    int len = strlen(s);


    while (i < len)
    {
        if (s[i] == '%')
        {
            if (i >= len-2)
                return -1;
            
            j = get_index(s[i+1]);
            k = get_index(s[i+2]);
            if (j < 0 || k < 0)
                return -2;
            
            if (sDecoded >= bufend)
                break;
            
            *sDecoded = (j << 4) +k;
            sDecoded ++;
            i += 3;
        }
        else
        {
            if (sDecoded >= bufend)
                break;
            *sDecoded = s[i ++];
            sDecoded ++;
        }
    }
    *sDecoded = 0;
    return 0;
    
}

StringPairList *CreateStringPairList(int iListNum)
{
    StringPairList *pNewList = NULL;

    if (iListNum < 0)
        return NULL;

    pNewList  = (StringPairList *)malloc(sizeof(StringPairList));
    if (pNewList == NULL)
        return NULL;

    pNewList->iLast = 0;
    pNewList->iMaxPairNum = iListNum;
    pNewList->pStrPairList = (StringPair *)malloc(sizeof(StringPair)*iListNum);
    if (pNewList->pStrPairList == NULL) {
        free( pNewList);
        return NULL;
    }
    return pNewList;
}

StringPairList *ConcatPairList(StringPairList *p1, const StringPairList *p2)
{
    int i = 0;
    assert(p1 != NULL);
    assert(p2 != NULL);


    for(; i < p2->iLast; i++) {
        if (p1->iLast >= p1->iMaxPairNum) 
            break;
        strcpy(p1->pStrPairList[p1->iLast].sFirst, 
            p2->pStrPairList[i].sFirst);
        strcpy(p1->pStrPairList[p1->iLast].sSecond,
            p2->pStrPairList[i].sSecond);
        
        p1->iLast++;
    }
    return p1;
}

void TraverseList(
    const StringPairList *pList, 
    int (*ProcessNameVal )(const char *, const char *, void *),
    void *pThis
    )
{
    int i;
    for(i = 0; i < pList->iLast; i++)
        ProcessNameVal(pList->pStrPairList[i].sFirst,
                pList->pStrPairList[i].sSecond, pThis);
}

void DestroyStringPairList(StringPairList *pList)
{
    assert(pList != NULL);
    free(pList->pStrPairList);
    free(pList);
}

void ResetStringPairList(StringPairList *pList)
{
    pList->iLast = 0;
}

inline const char *GetBinaryPara(
    const StringPairList *pPairList,
    const char *sName,
    char *sBinaryVal,
    size_t *BinaryLength
    )
{
    int i = 0;
    while( (i < pPairList->iLast )
            && strcasecmp(sName, pPairList->pStrPairList[i].sFirst))
        i ++;
    if (i >= pPairList->iLast) {
        strcpy(sBinaryVal, "");
        *BinaryLength = 0;
        return NULL;
    }
    else {
        decode_hexstring(pPairList->pStrPairList[i].sSecond,
            sBinaryVal, (int *)BinaryLength);
    }
    
    return sBinaryVal;
}

inline const char * GetParaValue(const StringPairList *pPairList, 
    const char *sName, char *sValue, size_t n)
{
    int i = 0;
    while( (i < pPairList->iLast )
            && strcasecmp(sName, pPairList->pStrPairList[i].sFirst))
        i ++;
    if (i >= pPairList->iLast) {
        strcpy(sValue, "");
        return NULL;
    }
    else {
        decode_string(pPairList->pStrPairList[i].sSecond,
            sValue, n);
    }
    
    return sValue;
}

// 设置二进制的
int SetBinaryPara(
    StringPairList *pPairList,
    const char *sName,
    const char *sBinaryVal,
    size_t length
    )
{
    char sEncodedValue[MAX_VALUE_LEN*2];
    
    if (pPairList->iLast >= pPairList->iMaxPairNum)
        return -1;
    
    encode_hexstring(sBinaryVal, length, sEncodedValue);
    
    strncpy(pPairList->pStrPairList[pPairList->iLast].sFirst, sName, MAX_NAME_LEN);
    pPairList->pStrPairList[pPairList->iLast].sFirst[MAX_NAME_LEN-1] = 0;

    strncpy(pPairList->pStrPairList[pPairList->iLast].sSecond, sEncodedValue, MAX_VALUE_LEN);
    pPairList->pStrPairList[pPairList->iLast].sSecond[MAX_VALUE_LEN-1] = 0;
    pPairList->iLast ++;
    
    return 0;

}

int    SetParaValue(StringPairList *pPairList,
    const char *sName,
    const char *sValue)
{
    char sEncodedValue[MAX_VALUE_LEN*2];
    
    if (pPairList->iLast >= pPairList->iMaxPairNum)
        return -1;
    
    encode_string(sValue, sEncodedValue);
    
    strncpy(pPairList->pStrPairList[pPairList->iLast].sFirst, sName, MAX_NAME_LEN);
    pPairList->pStrPairList[pPairList->iLast].sFirst[MAX_NAME_LEN-1] = 0;

    strncpy(pPairList->pStrPairList[pPairList->iLast].sSecond, sEncodedValue, MAX_VALUE_LEN);
    pPairList->pStrPairList[pPairList->iLast].sSecond[MAX_VALUE_LEN-1] = 0;
    pPairList->iLast ++;
    
    return 0;
}

int EncodePara(const StringPairList *pPairList, 
    char *sData,
    size_t  *pDataLen)
{
    int i, n;
    char *pBufStart = sData;
    char *pBufEnd = sData+*pDataLen;
    
    assert(pPairList != NULL);
    assert(sData != NULL);
    
    
    for(i = 0; i < pPairList->iLast; i++) {
        if (sData+strlen(pPairList->pStrPairList[i].sFirst)+
            strlen(pPairList->pStrPairList[i].sSecond)+2 >= pBufEnd)
            break;
            
        n = sprintf(sData, "%s=%s&", pPairList->pStrPairList[i].sFirst,
            pPairList->pStrPairList[i].sSecond);
        
        sData += n;
    }
    if (sData > pBufStart) {
        sData--;
        sData[0] = 0;
    }
    
    *pDataLen = (sData-pBufStart);
    
    return *pDataLen;
    
}



int DecodePara(const char* sRequest,
    int iReqLen,
    StringPairList *pPairList)
{
    int iRet = parsepara(sRequest,
        iReqLen,
        pPairList->pStrPairList,
        pPairList->iMaxPairNum,
        '=',
        '&');
    if (iRet >= 0) 
        pPairList->iLast = iRet;
    return (iRet >= 0?0:-1);
}


int parsepara(
    const char *sParaStr, 
    int    iStrLen,
    StringPair *pParaArr, 
    int iPairLen, 
    char ch1, 
    char ch2)
{
    int i;
//    char sName[MAX_NAME_LEN];
//    char sValue[MAX_VALUE_LEN];
    char *sName = NULL;
    char *sValue = NULL;
    char sBuffer[MAX_NAME_LEN+MAX_VALUE_LEN];
    char ch, chFlag;
    int iCounter;
    int iParaNum;
    StringPair *pMax;

    if (iStrLen == 0)
        return 0;
    
    /* default splitter is '=' and '&' */
    if (0 == ch1) ch1 = '=';
    if (0 == ch2) ch2 = '&';

    pMax = pParaArr+iPairLen-2;
    iCounter = 0;
    iParaNum = 0;
    chFlag = 0;

    sName = pParaArr->sFirst;
    sValue = pParaArr->sSecond;
    
    for(i = 0; i < iStrLen; i++)
    {
        ch = sParaStr[i];
            
        if (ch1 == ch)
        {
            if (iCounter >= MAX_NAME_LEN)
                iCounter = MAX_NAME_LEN-1;
            strncpy(sName, sBuffer, iCounter);
            sName[iCounter] = 0;
            iCounter = 0;
            chFlag = 1;
        }
        else if (ch2 == ch || 0 == ch)
        {
            if (iCounter >= MAX_VALUE_LEN)
                iCounter = MAX_VALUE_LEN-1;
            strncpy(sValue, sBuffer, iCounter);
            sValue[iCounter] = 0;
            iCounter = 0;
            if (chFlag != 1)
                return -1;
//            strcpy(pParaArr->sFirst, sName);
//            strcpy(pParaArr->sSecond, sValue);
            pParaArr ++, iParaNum++;
            sName = pParaArr->sFirst;
            sValue = pParaArr->sSecond;
            
            if (pParaArr >= pMax)
                break;
            chFlag = 0;
        }
        else
        {
            sBuffer[iCounter++] = ch;
        }
    }
    if (chFlag != 1)
        return -1;
    
    strncpy(sValue, sBuffer, iCounter);
    sValue[iCounter] = 0;
    pParaArr++;
    
    pParaArr->sFirst[0] = 0;
    pParaArr->sSecond[0] = 0;
    
    return ++iParaNum;
}

/*
const char *getval(const char *sName, const StringPair *pPairArr)
{
    while (pPairArr->sFirst[0])
    {
        if (!strcasecmp(sName, pPairArr->sFirst))
            return pPairArr->sSecond;
        pPairArr ++;
    }
    return "";
}
*/

int setval(const char *sName, 
    const char *sValue,
    StringPair *pPairArr)
{
    return 0;
}


void TrimLeft(char *s)
{
    char *p = s;
    
    while (*s)
    {
        if (isspace(*s)) 
            s++;
        else
        {
            memcpy(p, s, strlen(s));
            p[strlen(s)] = 0;
            break;
        }
    }
    if (!*s)
        *p = 0;
}

void TrimRight(char *s)
{
    char *p;

    p = (s+strlen(s)-1);
    while(p != s)
    {
        if (isspace(*p)) 
            *p --= 0;
        else
            break;
    }
}

void TrimString(char *s)
{
    TrimLeft(s);
    TrimRight(s);
}


int LoadFromFile(const char *sMsgFile, StringPair *pPairArr, const int iMaxNum)
{
        FILE *fp;
        char sLine[300];
        char sName[300], sValue[300];
        char *p;

        fp = fopen(sMsgFile, "r");
        if (fp == NULL) return -1;

        while (fgets(sLine, sizeof(sLine)-1, fp))
        {
            if (sLine[0] == '#' || (sLine[0] == '/' && sLine[1] == '/'))
                continue;    //Comment line
            TrimString(sLine);
            if (!sLine[0]) 
                continue;
            p = strchr(sLine, 0x20);
            if (!p) continue;
            strncpy(sName, sLine, p-sLine);
            sName[p-sLine] = 0;
            strcpy(sValue, p+1);
            TrimString(sName);
            TrimString(sValue);
            strncpy(pPairArr->sFirst, sName, sizeof(pPairArr->sFirst)-1);
            strncpy(pPairArr->sSecond, sValue, sizeof(pPairArr->sSecond)-1);
    //        TLib_Log_LogMsg("%s=%s", pPairArr->sFirst, pPairArr->sSecond);
            pPairArr ++;
        }
        pPairArr->sFirst[0] = 0;
        pPairArr->sSecond[0] = 0;
        return 0;
}

#ifdef _UNIT_TEST_

void PrintPairs(StringPair *pPairArr)
{
    while (pPairArr->sFirst[0])
    {
        printf("%s=%s\n", pPairArr->sFirst, pPairArr->sSecond);
        pPairArr ++;
    }
}

int main(int argc, char *argv[])
{
    char buffer[1024];
    int ret;

    if (argc < 3)
    {
        printf("usage:%s <de/en> <string>\n", argv[0]);
        return 1;
    }

    if (!strcmp(argv[1], "de"))
    {
        ret = decode_string(argv[2], buffer);
        printf("decode return=%d, (%s)\n", ret, buffer);
    }
    else
    {
        ret = encode_string(argv[2], buffer);
        printf("encode return=%d, (%s)\n", ret, buffer);
    }
    
    return 0;
}

#endif
 
