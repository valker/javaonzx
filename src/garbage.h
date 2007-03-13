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



extern int TemporaryRootsLength;
extern int GlobalRootsLength;
extern union cellOrPointer TemporaryRoots[MAXIMUM_TEMPORARY_ROOTS];
extern union cellOrPointer GlobalRoots[MAXIMUM_GLOBAL_ROOTS];

#define START_TEMPORARY_ROOTS   { int _tmp_roots_ = TemporaryRootsLength;
#define END_TEMPORARY_ROOTS      TemporaryRootsLength = _tmp_roots_;  }

#define IS_TEMPORARY_ROOT(_var_, _value_)                                \
  _var_ = (_var_ = _value_,                                              \
        TemporaryRoots[TemporaryRootsLength++].cellp = (cell *)&_var_,   \
        _var_)

#define DECLARE_TEMPORARY_ROOT(_type_, _var_, _value_)                   \
     _type_ IS_TEMPORARY_ROOT(_var_, _value_)



#endif