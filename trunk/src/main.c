/* project : javaonzx
   file    : main.c
   purpose : main module of javaonzx jvm
   author  : valker
*/

#include <intrz80.h>
#include <string.h>
#include "common.h"
#include "zx128hmem.h"

u1 class_image[]={
0
// here image of the test class will be included
};
non_banked void readHmem(u1* dest, far_ptr classImage, int count);

bool checkSignature(far_ptr* classImage) {
  static const u1 signature[] = {0xCA,0xFE,0xBA,0xBE};
  u1 buf[sizeof(signature)];
  readHmem(buf,*classImage,sizeof(signature));
  *classImage += sizeof(signature);
  return 0 == memcmp(buf, signature, sizeof(signature));
}

far_ptr loadClass(far_ptr classImage, int sizeOfClass)
{
  if(checkSignature(&classImage)) {
    
  }
  return 0;
}

void main(void)
{
  far_ptr class_info = loadClass(address_24_of(&class_image), sizeof(class_image));
}