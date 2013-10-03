#ifndef __CDC_HOTKEY_H_
#define __CDC_HOTKEY_H_

#include "GeneralHashFunctions.h"
#include <stdint.h>

typedef struct {
	uint32_t h1;
	uint32_t h2;
	uint32_t h3;
} t_hotkey;

#endif
