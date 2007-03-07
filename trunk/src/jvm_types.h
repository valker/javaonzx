/* project : javaonzx
   file    : jvm_types.h
   purpose : jvm common types
   author  : valker
*/

#ifndef JVM_TYPES_INCLUDED
#define JVM_TYPES_INCLUDED

#include "common.h"

////////////////////////////////////
// constant pool member structure
typedef struct {
    u1 tag;
    u1 *info;
} cp_info;

// constant pool tags
enum {
  CONSTANT_Class 	        = 7 ,
  CONSTANT_Fieldref 	        = 9 ,
  CONSTANT_Methodref 	        = 10 ,
  CONSTANT_InterfaceMethodref   = 11 ,
  CONSTANT_String 	        = 8 ,
  CONSTANT_Integer 	        = 3 ,
  CONSTANT_Float 	        = 4 ,
  CONSTANT_Long 	        = 5 ,
  CONSTANT_Double 	        = 6 ,
  CONSTANT_NameAndType 	        = 12 ,
  CONSTANT_Utf8 	        = 1
};

/////////////////////////////////////////
// attribute info structure
typedef struct  {
    u2 attribute_name_index;  // index in constant pool
    u4 attribute_length;      // length of attribute info
    u1 *info;
} attribute_info;

/////////////////////////////////////////
// field/method/class access flags
enum {
  ACC_PUBLIC 	    = 0x0001, 	//Declared public; may be accessed from outside its package. 
  ACC_PRIVATE 	    = 0x0002, 	//Declared private; usable only within the defining class. 
  ACC_PROTECTED     = 0x0004, 	//Declared protected; may be accessed within subclasses. 
  ACC_STATIC 	    = 0x0008, 	//Declared static. 
  ACC_FINAL 	    = 0x0010, 	//Declared final; no further assignment after initialization. 
  ACC_SYNCHRONIZED  = 0x0020, 	//Declared synchronized; invocation is wrapped in a monitor lock. 
  ACC_VOLATILE 	    = 0x0040, 	//Declared volatile; cannot be cached. 
  ACC_TRANSIENT     = 0x0080, 	//Declared transient; not written or read by a persistent object manager.
  ACC_NATIVE 	    = 0x0100, 	//Declared native; implemented in a language other than Java. 
  ACC_ABSTRACT 	    = 0x0400, 	//Declared abstract; no implementation is provided. 
  ACC_STRICT 	    = 0x0800 	//Declared strictfp; floating-point mode is FP-strict
};

/////////////////////////////////////////
// method info structure
typedef struct {
    u2 access_flags;
    u2 name_index;
    u2 descriptor_index;
    u2 attributes_count;
    attribute_info *attributes;
} method_info;

/////////////////////////////////////////
// field info structure
typedef struct  {
  u2 access_flags;
  u2 name_index;
  u2 descriptor_index;
  u2 attributes_count;
  attribute_info *attributes;
} field_info;

#endif
