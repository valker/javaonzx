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

bool checkSignature(far_ptr* classImage) {
  static const u1 signature[] = {0xCA,0xFE,0xBA,0xBE};
  u1 buf[sizeof(signature)];
  readHmem(buf,*classImage,sizeof(signature));
  *classImage += sizeof(signature);
  return 0 == memcmp(buf, signature, sizeof(signature));
}

far_ptr loadClass(far_ptr classImage)
{
  ClassInfo classInfo;
  if(checkSignature(&classImage)) {
    far_ptr p = hAlloc(sizeof(ClassInfo));
    if(p) {
      writeHmem(&classInfo, p, sizeof(classInfo));
    } else {
      // throw OutOfMemory
    }
    return p;
  }
  return 0;
}

void main(void)
{
  far_ptr class_info = loadClass(address_24_of(&class_image));
}