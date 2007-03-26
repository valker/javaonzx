/* project : javaonzx
   file    : native.h
   purpose : Native functions support
   author  : valker
*/

#ifndef NATIVE_H_INCLUDED
#define NATIVE_H_INCLUDED

#include "jvm_types.h"



typedef void NativeFunctionType(void);
typedef NativeFunctionType *NativeFunctionPtr;
//typedef far_ptr_of(NativeFunctionPtr) NativeFunctionPtr_FAR;

typedef NativeFunctionPtr NativeFunctionPtr_FAR;

typedef struct {
    PSTR_FAR name;
    PSTR_FAR signature;
    const NativeFunctionPtr implementation;
} NativeImplementationType;


typedef struct {
    PSTR_FAR packageName;
    PSTR_FAR baseName;
    const NativeImplementationType *const implementation;
} ClassNativeImplementationType;

NativeFunctionPtr_FAR getNativeFunction(INSTANCE_CLASS_FAR clazz, PSTR_FAR methodName, PSTR_FAR methodSignature);

extern const ClassNativeImplementationType nativeImplementations[];


#endif

