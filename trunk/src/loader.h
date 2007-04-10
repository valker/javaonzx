/* project : javaonzx
   file    : loaded.h
   purpose : class loaded
   author  : valker
*/

#ifndef LOADED_H_INCLUDED
#define LOADED_H_INCLUDED

#include "jvm_types.h"

#define JAVA_MIN_SUPPORTED_VERSION           45
#define JAVA_MAX_SUPPORTED_VERSION           48

enum validName_type {LegalMethod, LegalField, LegalClass};

void loadClassfile(INSTANCE_CLASS_FAR CurrentClass, BOOL fatalErrorIfFail);
FILEPOINTER_FAR    openClassfile(INSTANCE_CLASS_FAR clazz);
void    closeClassfile(FILEPOINTER_HANDLE_FAR);
i2      loadByteNoEOFCheck(FILEPOINTER_HANDLE_FAR);
u1      loadByte(FILEPOINTER_HANDLE_FAR);
u2      loadShort(FILEPOINTER_HANDLE_FAR);
u4      loadCell(FILEPOINTER_HANDLE_FAR);
void    loadBytes(FILEPOINTER_HANDLE_FAR, far_ptr buffer, u2 length);
BOOL    isValidName(PSTR_FAR name, enum validName_type);
void    skipBytes(FILEPOINTER_HANDLE_FAR, unsigned long i);
PSTR_FAR replaceLetters(PSTR_FAR string, char c1, char c2);

#endif

