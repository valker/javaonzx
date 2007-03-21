/* project : javaonzx
   file    : frame.c
   purpose : This file defines the internal operations for manipulating stack frames & performing exception handling.
   author  : valker
*/


/* Shortcuts to the errors/exceptions that the VM may throw */
/* These come directly from the CLDC Specification */

#include <stddef.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "jvm_types.h"
#include "class.h"
#include "root_code.h"
#include "garbage.h"
#include "seh.h"

const char ArithmeticException[]
= "java/lang/ArithmeticException";
const char ArrayIndexOutOfBoundsException[]
= "java/lang/ArrayIndexOutOfBoundsException";
const char ArrayStoreException[]
= "java/lang/ArrayStoreException";
const char ClassCastException[]
= "java/lang/ClassCastException";
const char ClassNotFoundException[]
= "java/lang/ClassNotFoundException";
const char IllegalAccessException[]
= "java/lang/IllegalAccessException";
const char IllegalArgumentException[]
= "java/lang/IllegalArgumentException";
const char IllegalMonitorStateException[]
= "java/lang/IllegalMonitorStateException";
const char IllegalThreadStateException[]
= "java/lang/IllegalThreadStateException";
const char IndexOutOfBoundsException[]
= "java/lang/IndexOutOfBoundsException";
const char InstantiationException[]
= "java/lang/InstantiationException";
const char InterruptedException[]
= "java/lang/InterruptedException";
const char NegativeArraySizeException[]
= "java/lang/NegativeArraySizeException";
const char NullPointerException[]
= "java/lang/NullPointerException";
const char NumberFormatException[]
= "java/lang/NumberFormatException";
const char RuntimeException[]
= "java/lang/RuntimeException";
const char SecurityException[]
= "java/lang/SecurityException";
const char StringIndexOutOfBoundsException[]
= "java/lang/StringIndexOutOfBoundsException";

const char IOException[]
= "java/io/IOException";


const char NoClassDefFoundError_[] = "java/lang/NoClassDefFoundError";
PSTR_FAR NoClassDefFoundError;

const char OutOfMemoryError_[] = "java/lang/OutOfMemoryError";
PSTR_FAR OutOfMemoryError;

const char VirtualMachineError[]
= "java/lang/VirtualMachineError";

/* The following classes would be needed for full JLS/JVMS compliance */
/* However, they are not part of the CLDC Specification. */

const char AbstractMethodError[]
= "java/lang/AbstractMethodError";
const char ClassCircularityError[]
= "java/lang/ClassCircularityError";
const char ClassFormatError[]
= "java/lang/ClassFormatError";
const char IllegalAccessError[]
= "java/lang/IllegalAccessError";
const char IncompatibleClassChangeError[]
= "java/lang/IncompatibleClassChangeError";
const char InstantiationError[]
= "java/lang/InstantiationError";
const char NoSuchFieldError[]
= "java/lang/NoSuchFieldError";
const char NoSuchMethodError[]
= "java/lang/NoSuchMethodError";
const char VerifyError[]
= "java/lang/VerifyError";

static THROWABLE_INSTANCE_FAR getExceptionInstance(PSTR_FAR name, PSTR_FAR msg);


void initFrame(void)
{
  NoClassDefFoundError.common_ptr_ = address_24_of(NoClassDefFoundError_);
  OutOfMemoryError.common_ptr_ = address_24_of(OutOfMemoryError_);
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
#if INCLUDEDEBUGCODE
    /* Turn off the allocation guard */
    NoAllocation = 0;
#endif /* INCLUDEDEBUGCODE */

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


static THROWABLE_INSTANCE_FAR getExceptionInstance(PSTR_FAR name, PSTR_FAR msg) {

                                                   INSTANCE_CLASS clazz;
                                                   THROWABLE_INSTANCE exception;

                                                   /* 
                                                   * For CLDC 1.0/1.1, we will hardcode the names of some of the 
                                                   * error classes that we don't support in CLDC.  This will prevent
                                                   * the KVM from trying to load error classes that don't actually
                                                   * exist.  This code should be removed if we ever decide to add the
                                                   * missing error classes to the KVM/CLDC.
                                                   */
                                                   if (name == AbstractMethodError          ||
                                                       name == ClassCircularityError        ||
                                                       name == ClassFormatError             ||
                                                       /* name == ExceptionInInitializerError  || */
                                                       name == IllegalAccessError           ||
                                                       name == IncompatibleClassChangeError ||
                                                       name == InstantiationError           ||
                                                       name == NoSuchMethodError            ||
                                                       name == NoSuchFieldError             ||
                                                       name == VerifyError) {

                                                           /* The VM is about to die; don't bother allocating memory from the heap */
                                                           char buffer[STRINGBUFFERSIZE];

                                                           sprintf(buffer, "%s", name);
                                                           if (msg != NULL) {
                                                               sprintf(buffer + strlen(buffer),": %s.", msg);
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
                                                   if (UnfoundException != NULL) {
                                                       /* The VM is about to die; don't bother allocating memory from the heap */
                                                       char buffer[STRINGBUFFERSIZE];

                                                       if (name == NoClassDefFoundError || name == ClassNotFoundException) {
                                                           sprintf(buffer,"%s", UnfoundException);
                                                       } 
                                                       else {
                                                           sprintf(buffer,"%s while loading exception class %s",
                                                               name, UnfoundException);
                                                           UnfoundExceptionMsg = msg;
                                                       }

                                                       if (UnfoundExceptionMsg != NULL) {
                                                           sprintf(buffer + strlen(buffer),": %s.",
                                                               UnfoundExceptionMsg);
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
                                                   clazz = (INSTANCE_CLASS)getClass(name);

                                                   START_TEMPORARY_ROOTS
                                                       IS_TEMPORARY_ROOT(exception, (THROWABLE_INSTANCE)instantiate(clazz));
                                                   /* The exception object instantiation is successful otherwise we
                                                   * will have already thrown an OutOfMemoryError */
#if PRINT_BACKTRACE
                                                   fillInStackTrace(&exception);
#endif
                                                   END_TEMPORARY_ROOTS

                                                       UnfoundException = NULL;
                                                   UnfoundExceptionMsg = NULL;

                                                   return exception;
}
