/* project : javaonzx
   file    : frame.h
   purpose : Definitions for execution frame & exception handling manipulation.
   author  : valker
*/

#ifndef FRAME_H_INCLUDED
#define FRAME_H_INCLUDED

#include "jvm_types.h"

extern PSTR_FAR OutOfMemoryError;
extern PSTR_FAR NoClassDefFoundError;

//void raiseExceptionWithMessage(const char* exceptionClassName, const char* exceptionMessage);
void raiseExceptionWithMessage(PSTR_FAR exceptionClassName, PSTR_FAR exceptionMessage);
void fatalError(const char* errorMessage);

#endif
