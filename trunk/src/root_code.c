/* project : javaonzx
   file    : root_code.c
   purpose : functions to be placed in root code
   author  : valker
*/

#include <string.h>
#include "common.h"
#include "zx128hmem.h"
#include "root_code.h"
#include "hashtable.h"

#define GETPAGE(fp) ((u1)(fp >> 16))




enum {
    MEM_PAGE_START_ADDR = 0xC000,
    MEM_PAGE_SIZE	= 0x4000
};

#ifdef ZX
#define GETMEMORY(n) ((void*)(n))
#else
#define GETMEMORY(n) (getMemory(n & 0x3FFF))
unsigned char * getMemory(int offset);
#endif


#ifdef ZX
#define MAKEPTR(p) ;
#else
#define MAKEPTR(p) (p = (p != 0) ? getPtr(p) : 0);
void* getPtr(void* p);
#endif
//
non_banked void initPage(void)
{
  ((HighMemoryDescriptor*)GETMEMORY(MEM_PAGE_START_ADDR))->type_ = GCT_FREE;
  ((HighMemoryDescriptor*)GETMEMORY(MEM_PAGE_START_ADDR))->size_ = (MEM_PAGE_SIZE - 2 * sizeof(HighMemoryDescriptor));

  {
    const u2 s = 0 - sizeof(HighMemoryDescriptor);
    ((HighMemoryDescriptor*)GETMEMORY(s))->type_ = GCT_END;
  }
}
//
const u1* PagesPtr;
u1 PagesCounter;
//
non_banked void hmemInit(const u1* pages, u1 pageCounter)
{
  const u1 stateMMU = getMMUState();
  PagesPtr = pages;
  PagesCounter = pageCounter;
  while(pageCounter) {
    setPage(*(pages++));
    initPage();
    --pageCounter;
  }
  restoreMMUState(stateMMU);
}
//
non_banked void* tryAlloc(u2 size) {
  HighMemoryDescriptor* h = (HighMemoryDescriptor*)GETMEMORY(MEM_PAGE_START_ADDR);
  void* p = 0;
  while(h->type_ != GCT_END) {
    if(h->type_ == GCT_FREE) {
      i2 delta = h->size_ - size;
      if(delta > 0) {
        h->type_ = GCT_NOPOINTERS;
        if(delta > sizeof(HighMemoryDescriptor)) {
          delta = h->size_;
          h->size_ = size;
          p = sizeof(HighMemoryDescriptor) + (u1*)h;
          h = (HighMemoryDescriptor*)((u1*)p + size);
          h->type_ = GCT_FREE;
          h->size_ = delta - size - sizeof(HighMemoryDescriptor);
          break;
        }
      }
    }
    h = (HighMemoryDescriptor*)(sizeof(HighMemoryDescriptor) + h->size_ + (u1*)h);
  }
  MAKEPTR(p);
  return p;
}
//
non_banked far_ptr hAlloc(u2 size) {
  far_ptr_conv r;
  u1 cnt = PagesCounter;
  const u1* pages = PagesPtr;
  const u1 mmu = getMMUState();
  void* ptr;
  r.ptr_ = 0;
  while(cnt > 0) {
    setPage(*pages);
    ptr = tryAlloc(size);
    if(ptr != 0) {
      r.structured_.offset_ = (u2)ptr;
      r.structured_.log_page_ = *pages;
      break;
    } else {
      ++pages;
      --cnt;
    }
  }
  restoreMMUState(mmu);
  return r.ptr_;
}
//
non_banked void hmemset(far_ptr ptr, u1 what, u2 howMany) {
  const u1 mmu = getMMUState();
  setPage(GETPAGE(ptr));
  memset(GETMEMORY(ptr),what,howMany);
  restoreMMUState(mmu);
}
//
non_banked void writeHmem(const void* src, far_ptr dest, u2 count) {
  const u1 mmu = getMMUState();
  setPage(GETPAGE(dest));
  memcpy((u1*)dest, src, count);
  restoreMMUState(mmu);
}
//
non_banked void readHmem(void* dest, far_ptr source, u2 count) {
    const u1 mmu = getMMUState();
    setPage(GETPAGE(source));
    memcpy(dest, (u1*)source, count);
    restoreMMUState(mmu);
}
//
//
non_banked void setBlockType(FPD ptr, GCT_ObjectType type, BOOL permanent) {
    const u1 mmu = getMMUState();
    setPage(GETPAGE(ptr.common_ptr_));
    {
        u1* pb = GETMEMORY(ptr.common_ptr_);
        ((HighMemoryDescriptor*)pb)->type_ = type;
        ((HighMemoryDescriptor*)pb)->permanent_ = permanent;
    }
    restoreMMUState(mmu);
}
//
//
non_banked u2     hstrlen(far_ptr ptr) {
    const u1 mmu = getMMUState();
    u2 r = 0;
    u1 tmp;
    setPage(GETPAGE(ptr));
    do {
      tmp = *((u1*)ptr);
    } while (tmp != 0 && (++r, ++ptr,1));
    restoreMMUState(mmu);
    return r;
}
//
/*=========================================================================
 * FUNCTION:      stringHash
 * OVERVIEW:      Returns a hash value for a C string
 * INTERFACE:
 *   parameters:  s:    pointer to a string
 *                len:  length of string, in bytes
 *   returns:     a hash value
 *=======================================================================*/
non_banked u2 stringHash(PSTR_FAR s, i2 length)
{
    const u1 mmu = getMMUState();
    u2 raw_hash = 0;
    const u1* p = (const u1*)(s.fields_.near_ptr_);
    setPage(s.fields_.page_);
    while (--length >= 0) { 
        i2 c = *p++;
        raw_hash =   (((raw_hash << 3) + raw_hash)  << 2) + raw_hash  + c;
    }
    restoreMMUState(mmu);
    return raw_hash;
}
//
#define BUF_SIZE 64 
non_banked i1       hmemcmp(far_ptr a1, far_ptr a2, u2 length)
{
    const u1 mmu = getMMUState();
    i1 r = 0;
    u1 buf[BUF_SIZE];

    while(length > 0) {
        u1 lengthNow = length > BUF_SIZE ? BUF_SIZE : length;
        setPage(GETPAGE(a1));
        memcpy(&buf[0], (void*) a1, lengthNow);
        setPage(GETPAGE(a2));
        r = memcmp(&buf[0], (void*) a2, lengthNow);
        if (r != 0) {
            break;
        }
        length -= BUF_SIZE;
        a1 += BUF_SIZE;
        a2 += BUF_SIZE;
    }
    restoreMMUState(mmu);
    return r;
}
non_banked void     hmemcpy(far_ptr dest, far_ptr src, u2 length) {
    const u1 mmu = getMMUState();
    u1 buf[BUF_SIZE];

    while(length > 0) {
        u1 lengthNow = length > BUF_SIZE ? BUF_SIZE : length;
        setPage(GETPAGE(src));
        memcpy(&buf[0], (void*) src, lengthNow);
        setPage(GETPAGE(dest));
        memcpy((void*)dest, &buf[0], lengthNow);
        length -= BUF_SIZE;
        src += BUF_SIZE;
        dest += BUF_SIZE;
    }
    restoreMMUState(mmu);
}
//
#define GET_TYPE_AT_IMPL(TYPE,FUNC_NAME)        \
    non_banked TYPE FUNC_NAME(far_ptr src) {    \
        const u1 mmu = getMMUState();           \
        TYPE r;                                 \
        setPage(GETPAGE(src));                  \
        r = *(const TYPE*)src;                  \
        restoreMMUState(mmu);                   \
        return r;                               \
    }
//
#define SET_TYPE_AT_IMPL(TYPE,FUNC_NAME)                    \
    non_banked void FUNC_NAME(far_ptr dest, TYPE data) {    \
        const u1 mmu = getMMUState();                       \
        setPage(GETPAGE(dest));                             \
        *(TYPE*)dest = data;                                \
        restoreMMUState(mmu);                               \
    }
//
GET_TYPE_AT_IMPL(u1,getCharAt)
GET_TYPE_AT_IMPL(u2,getWordAt)
GET_TYPE_AT_IMPL(u4,getDWordAt)
SET_TYPE_AT_IMPL(u1,setCharAt)
SET_TYPE_AT_IMPL(u2,setWordAt)
SET_TYPE_AT_IMPL(u4,setDWordAt)
//
non_banked i1       hstrcmp(far_ptr str1, far_ptr str2) {
    const u1 mmu = getMMUState();
    i1 r = 0;
    u1 buf[BUF_SIZE];

    u1 count;
    u1 count2;
    while(1) {
        setPage(GETPAGE(str1));
        count = BUF_SIZE;
        {
            u1* dest = &buf[0];
            const u1* source = (const u1*)str1;
            while (count && (*dest++ = *source++))    /* copy string */
                --count;
        }

        setPage(GETPAGE(str2));
        count2 = BUF_SIZE;
        {
            const u1* left = &buf[0];
            const u1* right = (const u1*)str2;
            while (count2 > count && (*left++ == *right++))
                --count2;
        }
    }
    restoreMMUState(mmu);
    return r;
}
//
//
non_banked far_ptr  hstrchr(far_ptr farString, char c )
{
    const u1 mmu = getMMUState();
    PSTR_FAR r;
    const u1* string = (const u1*) farString;

    setPage(GETPAGE(farString));

    while (*string && *string != c)
        string++;

    if (*string == c) {
        r.fields_.page_ = GETPAGE(farString);
        r.fields_.near_ptr_ = string;
    } else {
        r.common_ptr_ = 0;
    }
    restoreMMUState(mmu);
    return r.common_ptr_;
}
