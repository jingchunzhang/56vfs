/*
 **************************************************************************
 *                                                                        *
 *          General Purpose Hash Function Algorithms Library              *
 *                                                                        *
 * Author: Arash Partow - 2002                                            *
 * URL: http://www.partow.net                                             *
 * URL: http://www.partow.net/programming/hashfunctions/index.html        *
 *                                                                        *
 * Copyright notice:                                                      *
 * Free use of the General Purpose Hash Function Algorithms Library is    *
 * permitted under the guidelines and in accordance with the most current *
 * version of the Common Public License.                                  *
 * http://www.opensource.org/licenses/cpl1.0.php                          *
 *                                                                        *
 **************************************************************************
*/



#ifndef INCLUDE_GENERALHASHFUNCTION_C_H
#define INCLUDE_GENERALHASHFUNCTION_C_H


#include <stdio.h>
#include <stdint.h>
#include <string.h>


typedef unsigned int (*hash_function)(char*, unsigned int len);


unsigned int RSHash  (char* str, unsigned int len);
unsigned int JSHash  (char* str, unsigned int len);
unsigned int PJWHash (char* str, unsigned int len);
unsigned int ELFHash (char* str, unsigned int len);
unsigned int BKDRHash(char* str, unsigned int len);
unsigned int SDBMHash(char* str, unsigned int len);
unsigned int DJBHash (char* str, unsigned int len);
unsigned int DEKHash (char* str, unsigned int len);
unsigned int BPHash  (char* str, unsigned int len);
unsigned int FNVHash (char* str, unsigned int len);
unsigned int APHash  (char* str, unsigned int len);

void get_3_hash(char *s, uint32_t *h1, uint32_t *h2, uint32_t *h3);

#endif
