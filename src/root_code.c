/* project : javaonzx
   file    : root_code.c
   purpose : functions to be placed in root code
   author  : valker
*/

#include <string.h>
#include "common.h"
#include "zx128hmem.h"
#include "root_code.h"

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

non_banked void initPage(void)
{
  ((HighMemoryDescriptor*)GETMEMORY(MEM_PAGE_START_ADDR))->type_ = GCT_FREE;
  ((HighMemoryDescriptor*)GETMEMORY(MEM_PAGE_START_ADDR))->size_ = (MEM_PAGE_SIZE - 2 * sizeof(HighMemoryDescriptor));

  {
    const unsigned short int s = 0 - sizeof(HighMemoryDescriptor);
    ((HighMemoryDescriptor*)GETMEMORY(s))->type_ = GCT_END;
  }
}

const u1* PagesPtr;
u1 PagesCounter;

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

non_banked void hmemset(far_ptr ptr, u1 what, u2 howMany) {
  const u1 s = getMMUState();
  setPage(GETPAGE(ptr));
  memset(GETMEMORY(ptr),what,howMany);
  restoreMMUState(s);
}

non_banked void writeHmem(const u1* src, far_ptr dest, int count) {
  const u1 s = getMMUState();
  setPage(GETPAGE(dest));
  memcpy((u1*)dest, src, count);
  restoreMMUState(s);
}

non_banked void readHmem(u1* dest, far_ptr source, int count) {
    const u1 s = getMMUState();
    setPage(GETPAGE(source));
    memcpy(dest, (u1*)source, count);
    restoreMMUState(s);
}


non_banked void setBlockType(FPD ptr, GCT_ObjectType type) {
    const u1 s = getMMUState();
    setPage(GETPAGE(ptr.common_ptr_));
    {
        u1* pb = GETMEMORY(ptr.common_ptr_);
        ((HighMemoryDescriptor*)pb)->type_ = type;
    }
    restoreMMUState(s);
}