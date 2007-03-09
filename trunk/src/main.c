/* project : javaonzx
   file    : main.c
   purpose : main module of javaonzx jvm
   author  : valker
*/

#include <intrz80.h>
#include <string.h>
#include "common.h"
#include "zx128hmem.h"
#include "jvm_types.h"

u1 class_image[]={
//package v;
//public class t {
//    private t() {
//    }
//
//    public static void main(final String[] param) {
//        System.out.println("Hello world!");
//    }
//}
#include "test_class.h"
};
non_banked void readHmem(u1* dest, far_ptr classImage, int count);
non_banked far_ptr hAlloc(u2 size);
non_banked void writeHmem(const void* src, far_ptr dest, int count);
non_banked void hmemInit(const u1* pages, u1 pageCounter);


void initSystemClasses(void){
}

void main(void)
{
  // logical pages
  static const u1 pages[] = {2,3,4};
  hmemInit(&pages[0], sizeof(pages));
  initSystemClasses();
  {
    far_ptr class_info = loadClass(address_24_of(&class_image), "v/t");
  }
}