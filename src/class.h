/* project : javaonzx
   file    : class.h
   purpose : Internal runtime class structures
   author  : valker
*/

#ifndef CLASS_H_INCLUDED
#define CLASS_H_INCLUDED

#include "jvm_types.h"

extern THROWABLE_INSTANCE_FAR OutOfMemoryObject;

typedef union cellOrPointer {
    cell cell;
    cell *cellp;
    cell **cellpp;
    char *charp;
    char **charpp;
} cellOrPointer;

/* POINTERLIST */
struct pointerListStruct {
  long  length;
  cellOrPointer data[1];
};

#endif
