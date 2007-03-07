/* project : javaonzx
   file    : root_code.c
   purpose : functions to be placed in root code
   author  : valker
*/

#include "common.h"
#include "zx128hmem.h"

non_banked void readHmem(u1* dest, far_ptr classImage, int count) {
  while(count--) {
    *dest = getCharAt(classImage);
    ++classImage;
    ++dest;
  }
}