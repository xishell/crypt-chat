#ifndef BASETYPES_H
#define BASETYPES_H

/* Fasta heltalstyper */
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned long long uint64_t;

/* Storlekstyp – definieras bara om den inte redan finns */
#ifndef _SIZE_T_DEFINED
typedef unsigned long size_t;     // passa bättre för macOS 64-bit
#define _SIZE_T_DEFINED
#endif

/* Bool utan <stdbool.h> */
typedef unsigned char bool;
#define true  1
#define false 0

#ifndef NULL
#  define NULL ((void*)0)
#endif

#endif /* BASETYPES_H */