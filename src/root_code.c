/* project : javaonzx
   file    : root_code.c
   purpose : functions to be placed in root code
   author  : valker
*/

#include "common.h"
#include "zx128hmem.h"

/* High memory */


non_banked void readHmem(u1* dest, far_ptr classImage, int count) {
  while(count--) {
    *dest = getCharAt(classImage);
    ++classImage;
    ++dest;
  }
}

typedef enum {
    BT_EMPTY = 0,
    BT_END,
    BT_NOPOINTERS
} BlockType;

typedef struct {
  BlockType type_;
  u2        size_;
} HighMemoryDescriptor;

enum {
    MEM_PAGE_START_ADDR = 0xC000,
    MEM_PAGE_SIZE	= 0x4000
};

non_banked u1 getMMUState(void);
non_banked void restoreMMUState(u1);
non_banked void setPage(u1);

non_banked void initPage(void)
{
  ((HighMemoryDescriptor*)MEM_PAGE_START_ADDR)->type_ = BT_EMPTY;
  ((HighMemoryDescriptor*)MEM_PAGE_START_ADDR)->size_ = MEM_PAGE_SIZE - 2 * sizeof(HighMemoryDescriptor);

  ((HighMemoryDescriptor*)(0 - sizeof(HighMemoryDescriptor)))->type_ = BT_END;
  
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
  HighMemoryDescriptor* h = (HighMemoryDescriptor*)MEM_PAGE_START_ADDR;
  void* p = 0;
  while(h->type_ != BT_END) {
    if(h->type_ == BT_EMPTY) {
      i2 delta = h->size_ - size;
      if(delta > 0) {
        h->type_ = BT_NOPOINTERS;
        if(delta > sizeof(HighMemoryDescriptor)) {
          delta = h->size_;
          h->size_ = size;
          p = sizeof(HighMemoryDescriptor) + (u1*)h;
          h = (HighMemoryDescriptor*)((u1*)p + size);
          h->type_ = BT_EMPTY;
          h->size_ = delta - size - sizeof(HighMemoryDescriptor);
          break;
        }
      }
    }
    h = (HighMemoryDescriptor*)(sizeof(HighMemoryDescriptor) + h->size_ + (u1*)h);
  }
  return p;
}

non_banked far_ptr hAlloc(u2 size) {
  far_ptr r = 0;
  u1 cnt = PagesCounter;
  const u1* pages = PagesPtr;
  const u1 mmu = getMMUState();
  void* ptr;
  while(cnt > 0) {
    setPage(*pages);
    ptr = tryAlloc(size);
    if(ptr != 0) {
      // make r
      r = ((u2) ptr) | ((*pages) << 16);
      break;
    } else {
      ++pages;
      --cnt;
    }
  }
  restoreMMUState(mmu);
  return r;
}

non_banked void writeHmem(const void* src, far_ptr dest, int count) {
}