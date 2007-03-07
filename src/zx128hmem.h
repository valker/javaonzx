/* project : javaonzx
   file    : zx128hmem.h
   purpose : definitions of extended memory management functions
   author  : valentin pimenov
*/

#ifndef ZX128HMEM_INCLUDED_
#define ZX128HMEM_INCLUDED_

typedef unsigned long far_ptr;

/* returns byte at location from extended memory */
non_banked char getCharAt(far_ptr address24);

/* store byte at given location in extended memory 
   param: 3 low bytes of long - 24bit of address
          1 high byte of long - byte to be stored
*/
non_banked void setCharAt(long mix);

/* macro which make the mix from location and byte to be stored */
#define SETCHARAT(LOCATION,B) setCharAt((((unsigned long)((unsigned char)B)) << 24) | (LOCATION & 0x00FFFFFFL))

#endif