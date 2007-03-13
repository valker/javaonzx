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

typedef unsigned char   u1;
typedef signed char     i1;
typedef unsigned short  u2;
typedef signed   short  i2;
typedef unsigned long   u4;

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

#endif
