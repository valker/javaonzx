/* project : javaonzx
   file    : common.h
   purpose : main data types definition
   author  : valker
*/

#ifndef COMMON_H_INCLUDED
#define COMMON_H_INCLUDED

#ifndef ZX
#define non_banked
#endif

typedef unsigned char       u1;
typedef signed char         i1;
typedef unsigned short int  u2;
typedef signed short int    i2;
typedef unsigned long int   u4;
typedef signed long int     i4;

typedef enum {
    TRUE = 1,
    FALSE = 0
} BOOL;

typedef union {
  u2  word;
  u1  bytes[2];
  u1* pu1;
} wdecoder;

typedef union {
  u4  dword;
  u2  words[2];
} ldecoder;

#define COUNTOF(a) (sizeof(a)/sizeof(a[0]))
#endif
