/* project : javaonzx
   file    : fields.c
   purpose : Internal runtime structures for storing different kinds of fields
   author  : valker
*/

#include <stddef.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "fields.h"
#include "root_code.h"
#include "class.h"
#include "pool.h"
#include "frame.h"
#include "messages.h"

static void change_MethodSignature_to_KeyInternal(CONST_CHAR_HANDLE_FAR nameH, i2 *fromOffsetP, u1 **toP);


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


/*=========================================================================
* FUNCTION:      getNameAndTypeKey
*
* OVERVIEW:      converts a name and signature (either field or method) to 
*                a 32bit value. 
*
* INTERFACE:
*   parameters:  name:      Name of method
*                type:      Field or method signature.
*
*   returns:     The unique 32-bit value that represents the name and type.
* 
* We use a structure with two 16-bit values.  One 16-bit value encodes the
* name using standard name encoding.  The other holds the key of the
* signature.
* 
* Note that we cannot do the reverse operation.  Given a type key, we can't
* tell if its a method key or a field key!
*=======================================================================*/
NameTypeKey getNameAndTypeKey(PSTR_FAR name, PSTR_FAR type) {
    u2 typeLength = hstrlen(type.common_ptr_);
    NameTypeKey result;
    CONST_CHAR_HANDLE_FAR cchf;
    //if (INCLUDEDEBUGCODE && (inCurrentHeap(name) || inCurrentHeap(type))) { 
    //    fatalError(KVM_MSG_BAD_CALL_TO_GETNAMEANDTYPEKEY);
    //}
    //result.nt.nameKey = getUString(name)->key;
    result.nt.nameKey = getWordAt(getUString(name).common_ptr_ + UTF_KEY);
    cchf.common_ptr_ = (far_ptr)&type;
    result.nt.typeKey = (getCharAt(type.common_ptr_) == '(')
        ? change_MethodSignature_to_Key(cchf, 0, typeLength)
        : change_FieldSignature_to_Key(cchf, 0, typeLength);
    return result;
}


/*=========================================================================
* FUNCTION:      change_FieldSignature_to_Key()
*
* TYPE:          public instance-level operation on runtime classes
*
* OVERVIEW:      Converts a field signature into a unique 16-bit value.
*
* INTERFACE:
*   parameters:  type:        A pointer to the field signature
*                length:      length of signature
*
*   returns:     The unique 16-bit key that represents this field type
* 
* The encoding is as follows:
*    0 - 255:    Primitive types.  These use the same encoding as signature.
*                For example:  int is 'I', boolean is 'Z', etc.
*
*  256 - 0x1FFF  Non-array classes.  Every class is assigned a unique 
*                key value when it is created.  This value is used.  
*
* 0x2000-0xDFFF  Arrays of depth 1 - 6.  The high three bits represent the
*                depth, the low 13-bits (0 - 0x1FFF) represent the type.
*
* 0xE000-0xFFFF  Arrays of depth 8 or more.  These are specially encoded
*                so that the low 13-bits are different that the low 13-bits
*                of any other class.  The high bits are set so that we'll
*                recognize it as an array.
*
* Note that this scheme lets us encode an array class with out actually
* creating it.
* 
* The algorithm that gives key codes to classes cooperates with this scheme.
* When an array class is ever created, it is given the appropriate key code.
*
* NOTE! [Sheng 1/30/00] The verifier now uses the 13th bit (0x1000) to
* indicate whether a class instance has been initialized (ITEM_NewObject).
* As a result, only the low 12 bits (0 - 0xFFF) represent the base class
* type.
* 
*=======================================================================*/
FieldTypeKey change_FieldSignature_to_Key (CONST_CHAR_HANDLE_FAR typeH, i2 offset, i2 length) {
    CLASS_FAR clazz;
    PSTR_FAR start;
    PSTR_FAR type;
    PSTR_FAR end;
    PSTR_FAR p;
    i2 depth;

    start.common_ptr_ = getDWordAt(typeH.common_ptr_);
    type.common_ptr_ = start.common_ptr_ + offset;
    end.common_ptr_ = type.common_ptr_ + length;
    p.common_ptr_ = type.common_ptr_;

    /* Get the depth, and skip over the initial '[' that indicate the */

    while (getCharAt(p.common_ptr_) == '[') { 
        p.common_ptr_++;
    }
    depth = p.common_ptr_ - type.common_ptr_;

    /* Arrays of depth greater than 6 are just handled as if they were real
    * classes.  They will automatically be assigned */
    if (depth >= MAX_FIELD_KEY_ARRAY_DEPTH) { 
        clazz = getRawClassX(typeH, offset, length);
        /* I haven't decided yet whether these classes' keys will actually 
        * have their 3 high bits set to 7 or not.  The following will work
        * in either case */
        return getWordAt(clazz.common_ptr_ + CLASS_KEY) | (u2)(MAX_FIELD_KEY_ARRAY_DEPTH << FIELD_KEY_ARRAY_SHIFT);
    } else if (getCharAt(p.common_ptr_) != 'L') { 
        /* We have a primitive type, or an array thereof (depth <
        MAX_FIELD_KEY_ARRAY_DEPTH )
        */
        return getCharAt(p.common_ptr_) + (depth << FIELD_KEY_ARRAY_SHIFT);
    } else { 
        /* We have an object, or an array thereof.  With signatures, these
        * always have an 'L' at the beginning, and a ';' at the end.  
        * We're not checking that this is a well-formed signature, but we 
        * could */
        p.common_ptr_++;                    /* skip over 'L' for non-arrays */
        end.common_ptr_--;                  /* ignore ; at the end */
        clazz = getRawClassX(typeH, p.common_ptr_ - start.common_ptr_, end.common_ptr_ - p.common_ptr_);
        return getWordAt(clazz.common_ptr_ + CLASS_KEY) + (depth << FIELD_KEY_ARRAY_SHIFT);
    }
} 


/*=========================================================================
* FUNCTION:      change_MethodSignature_to_Key()
*
* TYPE:          public instance-level operation on runtime classes
*
* OVERVIEW:      Converts a method signature into a unique 16-bit value.
*
* INTERFACE:
*   parameters:  type:        A pointer to the method signature
*                length:      length of signature
*
*   returns:     The unique 16-bit key that represents this field type
* 
* The encoding is as follows:
* 
* We first encode the method signature into an "encoded" signature.  This
* encoded signature is then treated as it were a name, and that name.  We
* call change_Name_to_Key on it. 
* 
* The character array is created as follows
*   1 byte indicates the number of arguments
*   1 - 3 bytes indicate the first argument
*       ..
*   1 - 3 bytes indicate the nth argument
*   1 - 3 bytes indicate the return type.
* 
*  % If the type is primitive, it is encoded using its signature, like 'I',
*    'B', etc.  
*  % Otherwise, we get its 16-bit class key.  
*     .  If the high byte is in the range 'A'..'Z', we encode it as the 3
*        bytes,:  'L', hi, lo
*     .  Otherwise, we encode it as the 2 byte, hi, lo
*
* We're saving the whole range 'A'..'Z' since we may want to make 'O' be
* java.lang.Object, or 'X' be java.lang.String. . . .
* 
* Note that the first argument indicates slots, not arguments.  doubles and
* longs are two slots.
*=======================================================================*/
MethodTypeKey change_MethodSignature_to_Key(CONST_CHAR_HANDLE_FAR nameH, i2 offset, i2 length)
{

    /* We're first going to encode the signature, as indicated above. */
    u1 *buffer = (u1 *)str_buffer;/* temporary storage */

    /* unhand(nameH)[fromOffset] points into signature */
    i2 fromOffset = offset + 1;

    u1 *to = buffer + 1; /* pointer into encoding */
    i2 argCount = 0;
    i2 result;
    PSTR_FAR name;
    name.common_ptr_ = getDWordAt(nameH.common_ptr_);

    /* Call a helper function to do the actual work.  This help function
    * converts one field into its encoding.  fromOffset and to are updated.  
    * The function returns the number of slots of its argument */
    while (getCharAt(name.common_ptr_ + fromOffset) != ')') { 
        /* The following may GC */
        change_MethodSignature_to_KeyInternal(nameH, &fromOffset, &to);
        argCount++;
    }

    /* Skip over the ')' */
    fromOffset++;

    /* And interpret the return type, too */
    change_MethodSignature_to_KeyInternal(nameH, &fromOffset, &to);

    /* Set the slot type. */
    buffer[0] = argCount;
    if (fromOffset != offset + length) { 
        /* We should have exactly reached the end of the string */
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_METHOD_SIGNATURE);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }

    /* And get the name key for the encoded signature */
    {
        PSTR_FAR pf;
        CONST_CHAR_HANDLE_FAR cchf;
        pf.common_ptr_ = (far_ptr)&buffer;
        cchf.common_ptr_ = (far_ptr)&pf;
        result = change_Name_to_Key(cchf, 0, to - buffer);
    }
    return result;
}


/* Helper function used by the above function.  It parses the single field
* signature starting at *fromP, and puts the result at *toP.  Both are
* updated to just past where they were.  The size of the argument is returned.
*/
static void change_MethodSignature_to_KeyInternal(CONST_CHAR_HANDLE_FAR nameH, i2 *fromOffsetP, u1 **toP) {
    PSTR_FAR start;
    const i2 fromOffset = *fromOffsetP;
    PSTR_FAR from;
    u1 *to = *toP;
    PSTR_FAR endFrom;

    start.common_ptr_ = getDWordAt(nameH.common_ptr_);
    from.common_ptr_ = start.common_ptr_ + fromOffset;

    switch (getCharAt(from.common_ptr_)) { 
        default:
            endFrom.common_ptr_ = from.common_ptr_ + 1;
            break;
        case 'L':
            endFrom.common_ptr_ = hstrchr(from.common_ptr_ + 1, ';') + 1;
            break;
        case '[':
            endFrom.common_ptr_ = from.common_ptr_ + 1;
            while (getCharAt(endFrom.common_ptr_) == '[') endFrom.common_ptr_++;
            if (getCharAt(endFrom.common_ptr_) == 'L') { 
                endFrom.common_ptr_ = hstrchr(endFrom.common_ptr_ + 1, ';') + 1;             
            } else { 
                endFrom.common_ptr_++;
            }
            break;
    }
    if (endFrom.common_ptr_ == from.common_ptr_ + 1) { 
        //*to++ = (unsigned char)*from;
        *to++ = getCharAt(from.common_ptr_);
    } else { 
        FieldTypeKey key = change_FieldSignature_to_Key(nameH, from.common_ptr_ - start.common_ptr_, 
            endFrom.common_ptr_ - from.common_ptr_);
        unsigned char hiChar = ((unsigned)key) >> 8;
        unsigned char loChar = key & 0xFF;
        if (hiChar >= 'A' && hiChar <= 'Z') { 
            *to++ = 'L';
        }
        *to++ = hiChar;
        *to++ = loChar;
    }
    *to = '\0';                 /* always helps debugging */
    *toP = to;
    /* Although from and endfrom are no longer valid, their difference is
    * still the amount by which fromOffset has increased
    */
    *fromOffsetP = fromOffset + (endFrom.common_ptr_ - from.common_ptr_);
}
