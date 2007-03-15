/* project : javaonzx
   file    : jvm_types.h
   purpose : jvm common types
   author  : valker
*/

#ifndef JVM_TYPES_INCLUDED
#define JVM_TYPES_INCLUDED

#include "common.h"
#include "zx128hmem.h"

struct ClassInfo_ {
  char a;
};

//typedef unsigned long cell;
typedef far_ptr cell;

typedef struct ClassInfo_ ClassInfo;

////////////////////////////////////
// constant pool member structure
typedef struct {
    u1 tag;
    u1 *info;
} cp_info;

//// constant pool tags
//enum {
//  CONSTANT_Class 	        = 7 ,
//  CONSTANT_Fieldref 	        = 9 ,
//  CONSTANT_Methodref 	        = 10 ,
//  CONSTANT_InterfaceMethodref   = 11 ,
//  CONSTANT_String 	        = 8 ,
//  CONSTANT_Integer 	        = 3 ,
//  CONSTANT_Float 	        = 4 ,
//  CONSTANT_Long 	        = 5 ,
//  CONSTANT_Double 	        = 6 ,
//  CONSTANT_NameAndType 	        = 12 ,
//  CONSTANT_Utf8 	        = 1
//};

/////////////////////////////////////////
// attribute info structure
typedef struct  {
    u2 attribute_name_index;  // index in constant pool
    u4 attribute_length;      // length of attribute info
    u1 *info;
} attribute_info;

///////////////////////////////////////////
//// field/method/class access flags
//enum {
//  ACC_PUBLIC 	    = 0x0001, 	//Declared public; may be accessed from outside its package. 
//  ACC_PRIVATE 	    = 0x0002, 	//Declared private; usable only within the defining class. 
//  ACC_PROTECTED     = 0x0004, 	//Declared protected; may be accessed within subclasses. 
//  ACC_STATIC 	    = 0x0008, 	//Declared static. 
//  ACC_FINAL 	    = 0x0010, 	//Declared final; no further assignment after initialization. 
//  ACC_SYNCHRONIZED  = 0x0020, 	//Declared synchronized; invocation is wrapped in a monitor lock. 
//  ACC_VOLATILE 	    = 0x0040, 	//Declared volatile; cannot be cached. 
//  ACC_TRANSIENT     = 0x0080, 	//Declared transient; not written or read by a persistent object manager.
//  ACC_NATIVE 	    = 0x0100, 	//Declared native; implemented in a language other than Java. 
//  ACC_ABSTRACT 	    = 0x0400, 	//Declared abstract; no implementation is provided. 
//  ACC_STRICT 	    = 0x0800 	//Declared strictfp; floating-point mode is FP-strict
//};
//
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

/*=========================================================================
 * System-wide structure declarations
 *=======================================================================*/
 
 #define far_ptr_of(_ptr_type_) \
 union {                        \
  far_ptr common_ptr_;          \
  struct {                      \
    u2 near_ptr_;               \
    u1 page_;                   \
    u1 reserved;                \
  } fields_;                    \
 }
 
 
 typedef struct classStruct*                  CLASS;
 typedef far_ptr_of(CLASS)                    CLASS_FAR;

 typedef struct instanceClassStruct*          INSTANCE_CLASS;
 typedef far_ptr_of(INSTANCE_CLASS)           INSTANCE_CLASS_FAR;

 typedef struct arrayClassStruct*             ARRAY_CLASS;
 typedef far_ptr_of(ARRAY_CLASS)              ARRAY_CLASS_FAR;

 typedef struct objectStruct*                 OBJECT;
 typedef far_ptr_of(OBJECT)                   OBJECT_FAR;

 typedef struct instanceStruct*               INSTANCE;
 typedef far_ptr_of(INSTANCE)                 INSTANCE_FAR;

 typedef struct arrayStruct*                  ARRAY;
 typedef far_ptr_of(ARRAY)                    ARRAY_FAR;

 typedef struct stringInstanceStruct*         STRING_INSTANCE;
 typedef far_ptr_of(STRING_INSTANCE)          STRING_INSTANCE_FAR;

 typedef struct throwableInstanceStruct*      THROWABLE_INSTANCE;
 typedef far_ptr_of(THROWABLE_INSTANCE)       THROWABLE_INSTANCE_FAR;

 typedef struct internedStringInstanceStruct* INTERNED_STRING_INSTANCE;
 typedef far_ptr_of(INTERNED_STRING_INSTANCE) INTERNED_STRING_INSTANCE_FAR;

 typedef struct classInstanceStruct*          CLASS_INSTANCE;
 typedef far_ptr_of(CLASS_INSTANCE)           CLASS_INSTANCE_FAR;

 typedef struct byteArrayStruct*              BYTEARRAY;
 typedef far_ptr_of(BYTEARRAY)                BYTEARRAY_FAR;

 typedef struct shortArrayStruct*             SHORTARRAY;
 typedef far_ptr_of(SHORTARRAY)               SHORTARRAY_FAR;

 typedef struct pointerListStruct*            POINTERLIST;
 typedef far_ptr_of(POINTERLIST)              POINTERLIST_FAR;

 typedef struct weakPointerListStruct*        WEAKPOINTERLIST;
 typedef far_ptr_of(WEAKPOINTERLIST)          WEAKPOINTERLIST_FAR;

 typedef struct weakReferenceStruct*          WEAKREFERENCE;
 typedef far_ptr_of(WEAKREFERENCE)            WEAKREFERENCE_FAR;

 typedef struct fieldStruct*                  FIELD;
 typedef far_ptr_of(FIELD)                    FIELD_FAR;

 typedef struct fieldTableStruct*             FIELDTABLE;
 typedef far_ptr_of(FIELDTABLE)               FIELDTABLE_FAR;

 typedef struct methodStruct*                 METHOD;
 typedef far_ptr_of(METHOD)                   METHOD_FAR;

 typedef struct methodTableStruct*            METHODTABLE;
 typedef far_ptr_of(METHODTABLE)              METHODTABLE_FAR;

 typedef struct stackMapStruct*               STACKMAP;
 typedef far_ptr_of(STACKMAP)                 STACKMAP_FAR;

 typedef struct icacheStruct*                 ICACHE;
 typedef far_ptr_of(ICACHE)                   ICACHE_FAR;

 typedef struct chunkStruct*                  CHUNK;
 typedef far_ptr_of(CHUNK)                    CHUNK_FAR;

 typedef struct staticChunkStruct*            STATICCHUNK;
 typedef far_ptr_of(STATICCHUNK)              STATICCHUNK_FAR;

 typedef struct threadQueue*                  THREAD;
 typedef far_ptr_of(THREAD)                   THREAD_FAR;

 typedef struct javaThreadStruct*             JAVATHREAD;
 typedef far_ptr_of(JAVATHREAD)               JAVATHREAD_FAR;

 typedef struct monitorStruct*                MONITOR;
 typedef far_ptr_of(MONITOR)                  MONITOR_FAR;

 typedef struct stackStruct*                  STACK;
 typedef far_ptr_of(STACK)                    STACK_FAR;

 typedef struct frameStruct*                  FRAME;
 typedef far_ptr_of(FRAME)                    FRAME_FAR;

 typedef struct exceptionHandlerStruct*       HANDLER;
 typedef far_ptr_of(HANDLER)                  HANDLER_FAR;

 typedef struct exceptionHandlerTableStruct*  HANDLERTABLE;
 typedef far_ptr_of(HANDLERTABLE)             HANDLERTABLE_FAR;

 typedef struct filePointerStruct*            FILEPOINTER;
 typedef far_ptr_of(FILEPOINTER)              FILEPOINTER_FAR;

 typedef union constantPoolEntryStruct*       CONSTANTPOOL_ENTRY;
 typedef far_ptr_of(CONSTANTPOOL_ENTRY)       CONSTANTPOOL_ENTRY_FAR;

 typedef struct constantPoolStruct*           CONSTANTPOOL;
 typedef far_ptr_of(CONSTANTPOOL)             CONSTANTPOOL_FAR;

 typedef char*                                BYTES;
 typedef far_ptr_of(BYTES)                    BYTES_FAR;

 typedef FILEPOINTER        *                 FILEPOINTER_HANDLE;
 typedef far_ptr_of(FILEPOINTER_HANDLE)       FILEPOINTER_HANDLE_FAR;

 typedef OBJECT             *                 OBJECT_HANDLE;
 typedef far_ptr_of(OBJECT_HANDLE)            OBJECT_HANDLE_FAR;

 typedef INSTANCE           *                 INSTANCE_HANDLE;
 typedef far_ptr_of(INSTANCE_HANDLE)          INSTANCE_HANDLE_FAR;

 typedef ARRAY              *                 ARRAY_HANDLE;
 typedef far_ptr_of(ARRAY_HANDLE)             ARRAY_HANDLE_FAR;

 typedef BYTEARRAY          *                 BYTEARRAY_HANDLE;
 typedef far_ptr_of(BYTEARRAY_HANDLE)         BYTEARRAY_HANDLE_FAR;

 typedef POINTERLIST        *                 POINTERLIST_HANDLE;
 typedef far_ptr_of(POINTERLIST_HANDLE)       POINTERLIST_HANDLE_FAR;

 typedef WEAKPOINTERLIST    *                 WEAKPOINTERLIST_HANDLE;
 typedef far_ptr_of(WEAKPOINTERLIST_HANDLE)   WEAKPOINTERLIST_HANDLE_FAR;

 typedef JAVATHREAD         *                 JAVATHREAD_HANDLE;
 typedef far_ptr_of(JAVATHREAD_HANDLE)        JAVATHREAD_HANDLE_FAR;

 typedef BYTES              *                 BYTES_HANDLE;
 typedef far_ptr_of(BYTES_HANDLE)             BYTES_HANDLE_FAR;

 typedef METHOD             *                 METHOD_HANDLE;
 typedef far_ptr_of(METHOD_HANDLE)            METHOD_HANDLE_FAR;

 typedef FRAME              *                 FRAME_HANDLE;
 typedef far_ptr_of(FRAME_HANDLE)             FRAME_HANDLE_FAR;

 typedef const char *                         PSTR;
 typedef far_ptr_of(PSTR)                     PSTR_FAR;

 typedef PSTR_FAR           *                 CONST_CHAR_HANDLE;
 typedef far_ptr_of(CONST_CHAR_HANDLE)        CONST_CHAR_HANDLE_FAR;

 typedef unsigned char*     *                 UNSIGNED_CHAR_HANDLE;
 typedef far_ptr_of(UNSIGNED_CHAR_HANDLE)     UNSIGNED_CHAR_HANDLE_FAR;

 typedef STRING_INSTANCE    *                 STRING_INSTANCE_HANDLE;
 typedef far_ptr_of(STRING_INSTANCE_HANDLE)   STRING_INSTANCE_HANDLE_FAR;

 typedef THROWABLE_INSTANCE *                 THROWABLE_INSTANCE_HANDLE;
 typedef far_ptr_of(THROWABLE_INSTANCE_HANDLE) 
                                              THROWABLE_INSTANCE_HANDLE_FAR;

 typedef THREAD             *                 THREAD_HANDLE;
 typedef far_ptr_of(THREAD_HANDLE)            THREAD_HANDLE_FAR;



 
// #define BitSizeToByteSize(n)    (((n) + 7) >> 3)
// #define ByteSizeToCellSize(n)   (((n) + (CELL - 1)) >> log2CELL)
// #define StructSizeInCells(structName) ((sizeof(struct structName) + 3) >> 2)
// #define UnionSizeInCells(structName) ((sizeof(union structName) + 3) >> 2)
 
 /* Field and Method key types */
 typedef unsigned short NameKey;
 typedef unsigned short MethodTypeKey;
 typedef unsigned short FieldTypeKey;
 typedef union {
     struct {
             unsigned short nameKey;
             unsigned short typeKey; /* either MethodTypeKey or FieldTypeKey */
     } nt;
     unsigned long i;
 } NameTypeKey;
 
#define STRINGBUFFERSIZE  512

 /* Shared string buffer that is used internally by the VM */
 extern char str_buffer[STRINGBUFFERSIZE];

#define SHORTSIZE 2
#define CELL  4        /* Size of a java word (= 4 bytes) */

// TODO:
#define inAnyHeap(p) (1)


#endif
