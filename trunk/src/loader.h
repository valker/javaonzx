/* project : javaonzx
   file    : loaded.h
   purpose : class loaded
   author  : valker
*/

#ifndef LOADED_H_INCLUDED
#define LOADED_H_INCLUDED

#include "jvm_types.h"

void loadClassfile(INSTANCE_CLASS_FAR CurrentClass, BOOL fatalErrorIfFail);
FILEPOINTER_FAR    openClassfile(INSTANCE_CLASS_FAR clazz);
void closeClassfile(FILEPOINTER_HANDLE_FAR);
i1 loadByteNoEOFCheck(FILEPOINTER_HANDLE_FAR);
u1 loadByte(FILEPOINTER_HANDLE_FAR);
u2 loadShort(FILEPOINTER_HANDLE_FAR);

#endif

