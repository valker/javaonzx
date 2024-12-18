/* project : javaonzx
   file    : garbage.h
   purpose : memory management
   author  : valker
*/


#ifndef GARBAGE_H_INCLUDED
#define GARBAGE_H_INCLUDED

#include "jvm_types.h"
#include "class.h"

#define MAXIMUM_TEMPORARY_ROOTS 50
#define MAXIMUM_GLOBAL_ROOTS 20

/* The maximum number of callback functions that can be registered */
#ifndef CLEANUP_ROOT_SIZE
#define CLEANUP_ROOT_SIZE 16
#endif

#define unhand(x) (*(x))

extern int TemporaryRootsLength;
extern int GlobalRootsLength;
extern /*union*/ cellOrPointer TemporaryRoots[MAXIMUM_TEMPORARY_ROOTS];
extern /*union*/ cellOrPointer GlobalRoots[MAXIMUM_GLOBAL_ROOTS];

#define START_TEMPORARY_ROOTS   { int _tmp_roots_ = TemporaryRootsLength;
#define END_TEMPORARY_ROOTS      TemporaryRootsLength = _tmp_roots_;  }

#define IS_TEMPORARY_ROOT(_var_, _value_)                                \
  _var_ = (_var_.common_ptr_ = _value_.common_ptr_,                                              \
        TemporaryRoots[TemporaryRootsLength++] = _var_.common_ptr_,      \
        _var_)

#define IS_TEMPORARY_ROOT_(_var_, _value_)               \
    _var_ = (_var_ = _value_,                            \
    TemporaryRoots[TemporaryRootsLength++] = _var_,      \
    _var_)


#define DECLARE_TEMPORARY_ROOT(_type_, _var_, _value_)   \
     _type_ IS_TEMPORARY_ROOT(_var_, _value_)

#define DECLARE_TEMPORARY_ROOT_(_type_, _var_, _value_)  \
    _type_ IS_TEMPORARY_ROOT_(_var_, _value_)

void makeGlobalRoot(far_ptr object);
far_ptr mallocHeapObject(u2 size, GCT_ObjectType type);
far_ptr mallocObject(u2 size, GCT_ObjectType type);
far_ptr mallocBytes(u2 size);
void garbageCollect(i2 moreMemory);
far_ptr callocPermanentObject(u2 size);
far_ptr callocObject(u2 size, GCT_ObjectType type);
void setClassStatus(INSTANCE_CLASS_FAR clazz, int status);


#endif