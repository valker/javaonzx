/* project : javaonzx
   file    : frame.c
   purpose : This file defines the internal operations for manipulating stack frames & performing exception handling.
   author  : valker
*/


/* Shortcuts to the errors/exceptions that the VM may throw */
/* These come directly from the CLDC Specification */

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "jvm_types.h"
#include "class.h"
#include "root_code.h"
#include "garbage.h"
#include "seh.h"
#include "frame.h"
#include "main.h"

static PSTR_FAR UnfoundException = {0};
static PSTR_FAR UnfoundExceptionMsg = {0};

const char ArithmeticException[] = "java/lang/ArithmeticException";
const char ArrayIndexOutOfBoundsException[] = "java/lang/ArrayIndexOutOfBoundsException";
const char ArrayStoreException[] = "java/lang/ArrayStoreException";
const char ClassCastException[] = "java/lang/ClassCastException";
const char ClassNotFoundException_[] = "java/lang/ClassNotFoundException";
const char IllegalAccessException[] = "java/lang/IllegalAccessException";
const char IllegalArgumentException[] = "java/lang/IllegalArgumentException";
const char IllegalMonitorStateException[] = "java/lang/IllegalMonitorStateException";
const char IllegalThreadStateException[] = "java/lang/IllegalThreadStateException";
const char IndexOutOfBoundsException[] = "java/lang/IndexOutOfBoundsException";
const char InstantiationException[] = "java/lang/InstantiationException";
const char InterruptedException[] = "java/lang/InterruptedException";
const char NegativeArraySizeException[] = "java/lang/NegativeArraySizeException";
const char NullPointerException[] = "java/lang/NullPointerException";
const char NumberFormatException[] = "java/lang/NumberFormatException";
const char RuntimeException[] = "java/lang/RuntimeException";
const char SecurityException[] = "java/lang/SecurityException";
const char StringIndexOutOfBoundsException[] = "java/lang/StringIndexOutOfBoundsException";

const char IOException[] = "java/io/IOException";


const char NoClassDefFoundError_[] = "java/lang/NoClassDefFoundError";
PSTR_FAR NoClassDefFoundError;

const char OutOfMemoryError_[] = "java/lang/OutOfMemoryError";
PSTR_FAR OutOfMemoryError;

const char VirtualMachineError[]
= "java/lang/VirtualMachineError";

/* The following classes would be needed for full JLS/JVMS compliance */
/* However, they are not part of the CLDC Specification. */

const char AbstractMethodError_[] = "java/lang/AbstractMethodError";
const char ClassCircularityError_[] = "java/lang/ClassCircularityError";
const char ClassFormatError_[] = "java/lang/ClassFormatError";
const char IllegalAccessError_[] = "java/lang/IllegalAccessError";
const char IncompatibleClassChangeError_[] = "java/lang/IncompatibleClassChangeError";
const char InstantiationError_[] = "java/lang/InstantiationError";
const char NoSuchFieldError_[] = "java/lang/NoSuchFieldError";
const char NoSuchMethodError_[] = "java/lang/NoSuchMethodError";
const char VerifyError_[] = "java/lang/VerifyError";


PSTR_FAR AbstractMethodError;
PSTR_FAR ClassCircularityError;
PSTR_FAR ClassFormatError;
PSTR_FAR IllegalAccessError;
PSTR_FAR IncompatibleClassChangeError;
PSTR_FAR InstantiationError;
PSTR_FAR NoSuchFieldError;
PSTR_FAR NoSuchMethodError;
PSTR_FAR VerifyError;
PSTR_FAR ClassNotFoundException;


static THROWABLE_INSTANCE_FAR getExceptionInstance(PSTR_FAR name, PSTR_FAR msg);


void initFrame(void)
{
    NoClassDefFoundError.common_ptr_ = address_24_of(&NoClassDefFoundError_);
    OutOfMemoryError.common_ptr_ = address_24_of(&OutOfMemoryError_);

    AbstractMethodError.common_ptr_ = address_24_of(&AbstractMethodError_);
    ClassCircularityError.common_ptr_ = address_24_of(&ClassCircularityError_);
    ClassFormatError.common_ptr_ = address_24_of(&ClassFormatError_);
    IllegalAccessError.common_ptr_ = address_24_of(&IllegalAccessError_);
    IncompatibleClassChangeError.common_ptr_ = address_24_of(&IncompatibleClassChangeError_);
    InstantiationError.common_ptr_ = address_24_of(&InstantiationError_);
    NoSuchFieldError.common_ptr_ = address_24_of(&NoSuchFieldError_);
    NoSuchMethodError.common_ptr_ = address_24_of(&NoSuchMethodError_);
    VerifyError.common_ptr_ = address_24_of(&VerifyError_);

    ClassNotFoundException.common_ptr_ = address_24_of(&ClassNotFoundException_);
}

/*=========================================================================
* FUNCTION:      raiseExceptionWithMessage()
* TYPE:          internal exception handling operations
* OVERVIEW:      Raise an exception with a message.  This operation is 
*                intended to be used from within the VM only.  It should
*                be used for reporting only those exceptions and
*                errors that are known to be "safe" and "harmless",
*                i.e., the internal consistency and integrity of
*                the VM should not be endangered in any way.
*
*                Upon execution, the operation tries to load the
*                exception class with the given name, instantiate an
*                instance of that class, and
*                passes the object to the exception handler routines
*                of the VM. The operation will fail if the class can't
*                be found or there is not enough memory to load it or
*                create an instance of it.
* INTERFACE:
*   parameters:  exception class name
*                String message as a C string
*   returns:     <nothing>
* NOTE:          Since this operation needs to allocate two new objects
*                it should not be used for reporting memory-related
*                problems.
*=======================================================================*/
void raiseExceptionWithMessage(PSTR_FAR exceptionClassName, PSTR_FAR msg) {
    STRING_INSTANCE_FAR stringInstance;
    stringInstance.common_ptr_ = 0;
//#if INCLUDEDEBUGCODE
//    /* Turn off the allocation guard */
//    NoAllocation = 0;
//#endif /* INCLUDEDEBUGCODE */

    stringInstance = instantiateString(msg, hstrlen(msg.common_ptr_));
    if (stringInstance.common_ptr_ != 0) {
        START_TEMPORARY_ROOTS
            DECLARE_TEMPORARY_ROOT(STRING_INSTANCE_FAR, string, stringInstance);
            THROWABLE_INSTANCE_FAR exception = getExceptionInstance(exceptionClassName, msg);
            /* Store the string object into the exception */
            //exception->message = string;
            setDWordAt(exception.common_ptr_ + THROWABLE_MESSAGE, string.common_ptr_);
            THROW(exception);
        END_TEMPORARY_ROOTS
    }
    raiseException(exceptionClassName);
}

const u1 * copyStrToBuffer(u1* buffer, far_ptr str)
{
    const u2 nameLength = hstrlen(str);
    readHmem(buffer, str, nameLength);
    buffer[nameLength] = '\0';
    return buffer;
}
static THROWABLE_INSTANCE_FAR getExceptionInstance(PSTR_FAR name, PSTR_FAR msg) {
    INSTANCE_CLASS_FAR clazz;
    THROWABLE_INSTANCE_FAR exception;

    /* 
    * For CLDC 1.0/1.1, we will hardcode the names of some of the 
    * error classes that we don't support in CLDC.  This will prevent
    * the KVM from trying to load error classes that don't actually
    * exist.  This code should be removed if we ever decide to add the
    * missing error classes to the KVM/CLDC.
    */
    if (name.common_ptr_ == AbstractMethodError.common_ptr_          ||
       name.common_ptr_ == ClassCircularityError.common_ptr_        ||
       name.common_ptr_ == ClassFormatError.common_ptr_             ||
       /* name == ExceptionInInitializerError  || */
       name.common_ptr_ == IllegalAccessError.common_ptr_           ||
       name.common_ptr_ == IncompatibleClassChangeError.common_ptr_ ||
       name.common_ptr_ == InstantiationError.common_ptr_           ||
       name.common_ptr_ == NoSuchMethodError.common_ptr_            ||
       name.common_ptr_ == NoSuchFieldError.common_ptr_             ||
       name.common_ptr_ == VerifyError.common_ptr_) {

           /* The VM is about to die; don't bother allocating memory from the heap */
           char buffer[STRINGBUFFERSIZE];
           char buffer2[STRINGBUFFERSIZE];

           sprintf(buffer, "%s", copyStrToBuffer(&buffer2[0], name.common_ptr_));
           if (msg.common_ptr_ != 0) {
               sprintf(buffer + strlen(buffer),": %s.", copyStrToBuffer(&buffer2[0], msg.common_ptr_));
           } else {
               strcpy(buffer + strlen(buffer),".");
           }
           fatalError(buffer);
    }

    /*
    * This piece of code is executed when the VM has already tried to
    * throw an error or exception earlier, but couldn't find the error class.
    * The code basically converts raiseException semantics into fatalError 
    * semantics.
    */
    if (UnfoundException.common_ptr_ != 0) {
       /* The VM is about to die; don't bother allocating memory from the heap */
       char buffer[STRINGBUFFERSIZE];
       char buffer2[STRINGBUFFERSIZE];
       char buffer3[STRINGBUFFERSIZE];

       if (name.common_ptr_ == NoClassDefFoundError.common_ptr_ 
           || name.common_ptr_ == ClassNotFoundException.common_ptr_) {
           sprintf( buffer,
                    "%s", 
                    copyStrToBuffer(&buffer2[0], UnfoundException.common_ptr_));
       } 
       else {

           sprintf( buffer,
                    "%s while loading exception class %s", 
                    copyStrToBuffer(&buffer2[0], name.common_ptr_), 
                    copyStrToBuffer(&buffer3[0], UnfoundException.common_ptr_));
           UnfoundExceptionMsg = msg;
       }

       if (UnfoundExceptionMsg.common_ptr_ != 0) {
           sprintf(buffer + strlen(buffer),": %s.", copyStrToBuffer(&buffer2[0], UnfoundExceptionMsg.common_ptr_));
       } else {
           strcpy(buffer + strlen(buffer),".");
       }
       fatalError(buffer);
    }

    UnfoundException = name;
    UnfoundExceptionMsg = msg;

    /*
    * Load the actual exception/error class.
    * Any exceptions thrown during this call will be handled above.
    */
    clazz.common_ptr_ = getClass(name).common_ptr_;

    START_TEMPORARY_ROOTS
       IS_TEMPORARY_ROOT(exception, instantiate(clazz));
    /* The exception object instantiation is successful otherwise we
    * will have already thrown an OutOfMemoryError */
    //#if PRINT_BACKTRACE
    //fillInStackTrace(&exception);
    //#endif
    END_TEMPORARY_ROOTS

    UnfoundException.common_ptr_ = 0;
    UnfoundExceptionMsg.common_ptr_ = 0;

    return exception;
}


/*=========================================================================
* FUNCTION:      raiseException()
* TYPE:          internal exception handling operation
* OVERVIEW:      Raise an exception.  This operation is intended
*                to be used from within the VM only.  It should
*                be used for reporting only those exceptions and
*                errors that are known to be "safe" and "harmless",
*                i.e., the internal consistency and integrity of
*                the VM should not be endangered in any way.
*
*                Upon execution, the operation tries to load the
*                exception class with the given name, instantiate an
*                instance of that class, and
*                passes the object to the exception handler routines
*                of the VM. The operation will fail if the class can't
*                be found or there is not enough memory to load it or
*                create an instance of it.
* INTERFACE:
*   parameters:  exception class name
*   returns:     <nothing>
* NOTE:          Since this operation needs to allocate two new objects
*                it should not be used for reporting memory-related
*                problems.
*=======================================================================*/
void raiseException(PSTR_FAR exceptionClassName)
{
//#if INCLUDEDEBUGCODE
//    /* Turn off the allocation guard */
//    NoAllocation = 0;
//#endif /* INCLUDEDEBUGCODE */
    PSTR_FAR msg = {0};
    THROW(getExceptionInstance(exceptionClassName, msg))
}


/*=========================================================================
* FUNCTION:      fatalError()
* TYPE:          internal error handling operation
* OVERVIEW:      Report a fatal error indicating that the execution
*                of erroneous Java code might have endangered the
*                integrity of the VM. VM will be stopped. This
*                operation should be called only the from inside the VM.
* INTERFACE:
*   parameters:  error message string.
*   returns:     <nothing>
*=======================================================================*/
void fatalError(const char* errorMessage)
{
    //if (INCLUDEDEBUGCODE) {
    //    printVMstatus();
    //}
    AlertUser(errorMessage);
    VM_EXIT(FATAL_ERROR_EXIT_CODE);
}
