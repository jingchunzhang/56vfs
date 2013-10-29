#ifndef __NM_C_API_H_
#define __NM_C_API_H_
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <math.h>
#include <stdint.h>
#include <ctype.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif
void SetInt(uint32_t key, uint32_t val);
void IncInt(uint32_t key, uint32_t val);
void SetStr(uint32_t key, char * val);

void touch_timestamp();
#ifdef __cplusplus
}
#endif
#endif
