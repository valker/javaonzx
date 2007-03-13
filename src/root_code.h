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

non_banked void     readHmem(u1* dest, far_ptr classImage, int count);
non_banked far_ptr  hAlloc(u2 size);
non_banked u1       getMMUState(void);
non_banked void     restoreMMUState(u1);
non_banked void     setPage(u1);
non_banked void     hmemInit(const u1* pages, u1 pageCounter);
non_banked void     hmemset(far_ptr ptr, u1 what, u2 howMany);
non_banked void     setBlockType(FPD ptr, GCT_ObjectType type);

#endif
