#ifndef _TURF_CRC32_H_
#define _TURF_CRC32_H_

#include <stdint.h>
#include <unistd.h>

#include "misc.h"

uint32_t _API crc32(const void* buf, size_t size);

#endif  // _TURF_CRC32_H_
