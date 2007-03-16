/* project : javaonzx
   file    : root_code.h
   purpose : functions to be placed in root code (low memory)
   author  : valker
*/

#ifndef ROOT_CODE_H_INCLUDED
#define ROOT_CODE_H_INCLUDED

#include "jvm_types.h"

typedef HighMemoryDescriptor * PD;
typedef far_ptr_of(PD) FPD;

non_banked void     hmemInit(const u1* pages, u1 pageCounter);
non_banked void     readHmem(void* dest, far_ptr src, u2 count);
non_banked void     writeHmem(const void* src, far_ptr dest, u2 count);
non_banked u1       getCharAt(far_ptr src);
non_banked void     setCharAt(far_ptr dest, u1 c);
non_banked u2       getWordAt(far_ptr src);
non_banked void     setWordAt(far_ptr dest, u2 w);
non_banked u4       getDWordAt(far_ptr src);
non_banked void     setDWordAt(far_ptr dest, u4 dw);

non_banked far_ptr  hAlloc(u2 size);

non_banked u1       getMMUState(void);
non_banked void     restoreMMUState(u1);
non_banked void     setPage(u1);
non_banked void     setBlockType(FPD ptr, GCT_ObjectType type);

non_banked void     hmemset(far_ptr ptr, u1 what, u2 howMany);
non_banked u2       hstrlen(far_ptr ptr);
non_banked i1       hmemcmp(far_ptr ptr1, far_ptr ptr2, u2 length);
non_banked void     hmemcpy(far_ptr dest, far_ptr src, u2 length);
non_banked i1       hstrcmp(far_ptr str1, far_ptr str2);

#ifndef ZX
far_ptr address_24_of(const void* ptr);
#endif

#endif
