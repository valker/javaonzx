/* project : javaonzx
   file    : pool.h
   purpose : constant pool
   author  : valker
*/

#include <stdio.h>
#include <stddef.h>
#include "pool.h"
#include "jvm_types.h"
#include "garbage.h"
#include "messages.h"
#include "frame.h"
#include "root_code.h"

BOOL classHasAccessToClass(INSTANCE_CLASS_FAR currentClass, CLASS_FAR targetClass);


/*=========================================================================
* FUNCTION:      verifyClassAccess
*                classHasAccessToClass
*                classHasAccessToMember
* TYPE:          public instance-level operation
* OVERVIEW:      Helper functions for checking access rights
*                in field/method resolution.
* INTERFACE:
*   parameters:
*   returns:
*   throws:      IllegalAccessError on failure
*                (this class is not part of CLDC 1.0 or 1.1)
*=======================================================================*/
void verifyClassAccess(CLASS_FAR targetClass, INSTANCE_CLASS_FAR currentClass) 
{
    CLASS_FAR cf;
    char buf1[STRINGBUFFERSIZE];
    char buf2[STRINGBUFFERSIZE];
    cf.common_ptr_ = currentClass.common_ptr_;
    if (!classHasAccessToClass(currentClass, targetClass)) { 
        START_TEMPORARY_ROOTS
            DECLARE_TEMPORARY_ROOT(PSTR_FAR, targetName, getClassName(targetClass));
            DECLARE_TEMPORARY_ROOT(PSTR_FAR, currentName, getClassName(cf));

            sprintf(str_buffer, 
                    KVM_MSG_CANNOT_ACCESS_CLASS_FROM_CLASS_2STRPARAMS, 
                    copyStrToBuffer(buf1, targetName.common_ptr_), 
                    copyStrToBuffer(buf2, currentName.common_ptr_));
        END_TEMPORARY_ROOTS
        {
            PSTR_FAR pf;
            pf.common_ptr_ = (far_ptr) &str_buffer;
            raiseExceptionWithMessage(IllegalAccessError, pf);
        }
    }
}

BOOL classHasAccessToClass(INSTANCE_CLASS_FAR currentClass, CLASS_FAR targetClass) { 
    if (    currentClass.common_ptr_ == 0 
        || (currentClass.common_ptr_ == targetClass.common_ptr_)
        /* Note that array classes have the same package and access as
        * their base classes */
        || (getWordAt(targetClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_PUBLIC)
        || (getDWordAt(targetClass.common_ptr_ + CLASS_PACKAGENAME) == getDWordAt(currentClass.common_ptr_ + CLASS_PACKAGENAME))
        ) { 
            return TRUE;
    } else { 
        return FALSE;
    }
}
