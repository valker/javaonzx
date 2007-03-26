/* project : javaonzx
   file    : common.h
   purpose : Native function interface
   author  : valker
*/

#include <stddef.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "native.h"
#include "hashtable.h"
#include "root_code.h"
#include "class.h"

static i2 xstrcmp(PSTR_FAR s1, PSTR_FAR s2);

const char emptyString[] = "";

/*=========================================================================
* FUNCTION:      getNativeFunction()
* TYPE:          lookup operation
* OVERVIEW:      Given a function name as a string, try to find
*                a corresponding native function and return a
*                a pointer to it if found.
* INTERFACE:
*   parameters:  class name, method name
*   returns:     function pointer or NIL if not found.
*=======================================================================*/
NativeFunctionPtr_FAR getNativeFunction(INSTANCE_CLASS_FAR clazz, PSTR_FAR methodName, PSTR_FAR methodSignature) {
    const ClassNativeImplementationType *cptr;
    const NativeImplementationType *mptr;
    UString_FAR UBaseName;
    UString_FAR UPackageName;
    PSTR_FAR baseName;
    PSTR_FAR packageName;
    //UBaseName    = clazz->clazz.baseName;
    UBaseName.common_ptr_ = getDWordAt(clazz.common_ptr_ + CLASS_BASENAME);
    //UPackageName = clazz->clazz.packageName;
    UPackageName.common_ptr_ = getDWordAt(clazz.common_ptr_ + CLASS_PACKAGENAME);

    /* Package names can be NULL -> must do an explicit check */
    /* to ensure that string comparison below will not fail */
    if (UPackageName.common_ptr_ == 0) {
        packageName.common_ptr_ = address_24_of(&emptyString);
    } else {
        packageName = UStringInfo(UPackageName);
    }

    if (UBaseName.common_ptr_ == 0) {
        baseName.common_ptr_ = address_24_of(&emptyString);
    } else {
        baseName = UStringInfo(UBaseName);
    }

    for (cptr = nativeImplementations; cptr->baseName.common_ptr_ != 0 ; cptr++) {                 
        if (   (xstrcmp(cptr->packageName, packageName) == 0) 
            && (xstrcmp(cptr->baseName, baseName) == 0)) { 
                break;
        }
    }

    for (mptr = cptr->implementation; mptr != NULL ; mptr++) {
        PSTR_FAR name = mptr->name;
        if (name.common_ptr_ == 0) {
            return 0;
        }
        if (hstrcmp(name.common_ptr_, methodName.common_ptr_) == 0) {
            PSTR_FAR signature = mptr->signature;
            /* The signature is NULL for non-overloaded native methods. */
            if (signature.common_ptr_ == 0|| (xstrcmp(signature, methodSignature) == 0)){
                return mptr->implementation;
            }
        }
    }

    return 0;
}

static i2 xstrcmp(PSTR_FAR s1, PSTR_FAR s2) {
    if (s1.common_ptr_ == 0) {
        s1.common_ptr_ = address_24_of(&emptyString);
    }
    if (s2.common_ptr_ == 0) {
        s2.common_ptr_ = address_24_of(&emptyString);
    }
    return hstrcmp(s1.common_ptr_, s2.common_ptr_);
}
