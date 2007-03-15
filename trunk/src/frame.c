/* project : javaonzx
   file    : frame.c
   purpose : This file defines the internal operations for manipulating stack frames & performing exception handling.
   author  : valker
*/


/* Shortcuts to the errors/exceptions that the VM may throw */
/* These come directly from the CLDC Specification */

#ifdef ZX
#include <intrz80.h>
#endif
#include "jvm_types.h"

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

void initFrame(void)
{
  NoClassDefFoundError.common_ptr_ = address_24_of(&NoClassDefFoundError_);
  OutOfMemoryError.common_ptr_ = address_24_of(&OutOfMemoryError_);
}
