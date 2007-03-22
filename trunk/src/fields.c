/* project : javaonzx
   file    : fields.c
   purpose : Internal runtime structures for storing different kinds of fields
   author  : valker
*/

#include <stddef.h>
#include "fields.h"
#include "root_code.h"
#include "class.h"
#include "pool.h"

/*=========================================================================
* FUNCTION:      getSpecialMethod()
* TYPE:          public instance-level operation
* OVERVIEW:      Find a specific special method (<clinit>, main)
*                using a simple linear lookup strategy.
* INTERFACE:
*   parameters:  class pointer, method name and signature pointers
*   returns:     pointer to the method or NIL
*=======================================================================*/

METHOD_FAR getSpecialMethod(INSTANCE_CLASS_FAR thisClass, NameTypeKey key) {
    METHODTABLE_FAR methodTable;
    methodTable.common_ptr_ = getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_METHODTABLE);
    FOR_EACH_METHOD(thisMethod, methodTable) 
        if (    (getWordAt(thisMethod.common_ptr_ + METHOD_ACCESSFLAGS) & ACC_STATIC)
            && (getDWordAt(thisMethod.common_ptr_ + METHOD_NAMETYPEKEY) == key.i))
            return thisMethod;
    END_FOR_EACH_METHOD
    {
        METHOD_FAR mf;
        mf.common_ptr_ = 0;
        return mf;
    }
}
