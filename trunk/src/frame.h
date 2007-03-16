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
extern PSTR_FAR ClassCircularityError;
extern PSTR_FAR ClassFormatError;
extern PSTR_FAR IncompatibleClassChangeError;
extern PSTR_FAR VerifyError;
extern PSTR_FAR ClassNotFoundException;

//void raiseExceptionWithMessage(const char* exceptionClassName, const char* exceptionMessage);
void raiseExceptionWithMessage(PSTR_FAR exceptionClassName, PSTR_FAR exceptionMessage);
void fatalError(const char* errorMessage);
void raiseException(PSTR_FAR exceptionClassName);

#endif
