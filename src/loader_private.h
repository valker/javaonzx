/* project : javaonzx
   file    : loader_private.h
   purpose : private header of loader
   author  : valker
*/

#ifndef LOADER_PRIVATE_H_INCLUDED
#define LOADER_PRIVATE_H_INCLUDED

#include "loader.h"

#define RECOGNIZED_CLASS_FLAGS (ACC_PUBLIC | \
    ACC_FINAL | \
    ACC_SUPER | \
    ACC_INTERFACE | \
    ACC_ABSTRACT)

#define RECOGNIZED_FIELD_FLAGS (ACC_PUBLIC | \
    ACC_PRIVATE | \
    ACC_PROTECTED | \
    ACC_STATIC | \
    ACC_FINAL | \
    ACC_VOLATILE | \
    ACC_TRANSIENT)

#define RECOGNIZED_METHOD_FLAGS (ACC_PUBLIC | \
    ACC_PRIVATE | \
    ACC_PROTECTED | \
    ACC_STATIC | \
    ACC_FINAL | \
    ACC_SYNCHRONIZED | \
    ACC_NATIVE | \
    ACC_ABSTRACT | \
    ACC_STRICT )



void loadRawClass(INSTANCE_CLASS_FAR CurrentClass, BOOL fatalErrorIfFail);
INSTANCE_CLASS_FAR findSuperMostUnlinked(INSTANCE_CLASS_FAR clazz);
POINTERLIST_FAR loadConstantPool(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
void loadVersionInfo(FILEPOINTER_HANDLE_FAR ClassFileH);
void loadClassInfo(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
void loadInterfaces(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
void loadFields(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, POINTERLIST_HANDLE_FAR StringPoolH);
void loadMethods(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, POINTERLIST_HANDLE_FAR StringPoolH);
void ignoreAttributes(FILEPOINTER_HANDLE_FAR ClassFile, POINTERLIST_HANDLE_FAR StringPool);
void verifyUTF8String(far_ptr bytes, unsigned short length);
PSTR_FAR getUTF8String(POINTERLIST_HANDLE_FAR StringPoolH, u2 index);
void verifyName(PSTR_FAR name, enum validName_type type);
u2 verifyMethodType(PSTR_FAR name, PSTR_FAR signature);
void verifyFieldType(PSTR_FAR type);
PSTR_FAR skipOverFieldType(PSTR_FAR string, BOOL void_okay, u2 length);
PSTR_FAR skipOverFieldName(PSTR_FAR string, BOOL slash_okay, u2 length);
void loadOneMethod(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, METHOD_FAR* thisMethodH, POINTERLIST_HANDLE_FAR StringPoolH);
void verifyMethodFlags(u2 flags, u2 classFlags, PSTR_FAR name);
void loadMethodAttributes(FILEPOINTER_HANDLE_FAR ClassFileH, METHOD_FAR* thisMethodH, POINTERLIST_HANDLE_FAR StringPoolH);
u2 loadCodeAttribute(FILEPOINTER_HANDLE_FAR ClassFileH, METHOD_FAR* thisMethodH, POINTERLIST_HANDLE_FAR StringPoolH);
void verifyConstantPoolEntry(INSTANCE_CLASS_FAR CurrentClass, u2 index, u1 tag);
u2 loadExceptionHandlers(FILEPOINTER_HANDLE_FAR ClassFileH, METHOD_FAR* thisMethodH);
u4 loadStackMaps(FILEPOINTER_HANDLE_FAR ClassFileH, METHOD_FAR* thisMethodH);
void verifyClassFlags(u2 flags);
void verifyFieldFlags(u2 flags, u2 classFlags);
void loadStaticFieldAttributes(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, FIELD_FAR thisField, POINTERLIST_HANDLE_FAR StringPoolH);

#endif
