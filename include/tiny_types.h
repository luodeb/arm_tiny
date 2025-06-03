
#ifndef __TYPES_H__
#define __TYPES_H__

typedef unsigned char        uint8_t;
typedef unsigned short       uint16_t;
typedef unsigned int         uint32_t;
typedef unsigned long long   uint64_t;
typedef unsigned long int	uintptr_t;

typedef    _Bool  bool;

#define false 0
#define true  1

#define SIZE_MAX		(9223372036854775807UL)

#define MIN(a, b)		((a) < (b) ? (a) : (b))
#define MAX(a, b)		((a) > (b) ? (a) : (b))

#define vaddr_t uint64_t
#define paddr_t uint64_t

#endif