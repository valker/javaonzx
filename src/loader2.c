/* project : javaonzx
   file    : locader2.c
   purpose : class loader 2-nd part
   author  : valker
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "jvm_types.h"
#include "loader.h"
#include "root_code.h"
#include "class.h"
#include "seh.h"
#include "messages.h"
#include "frame.h"
#include "pool.h"
#include "fields.h"
#include "native.h"
#include "loader_private.h"

u2 readClassStatus(INSTANCE_CLASS_FAR clazz) {
    return getWordAt(clazz.common_ptr_ + INSTANCE_CLASS_STATUS);
}
//
//
///*=========================================================================
//* FUNCTION:      loadStaticFieldAttributes()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the "ConstantValue" attribute of a static field,
//*                ignoring all the other possible field attributes.
//* INTERFACE:
//*   parameters:  classfile pointer, constant pool pointer,
//*                field attribute count, pointer to the runtime field struct
//*   returns:     <nothing>
//*   throws:      ClassFormatError if there is any problem with the format
//*                of the ConstantValue attribute or there is more than one
//*                such attribute
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*
//* COMMENTS: this function sets the u.offset field of the field to the
//*           constant pool entry that should contain its initial value, or
//*           to 0 if there is no initial value.
//*=======================================================================*/
void loadStaticFieldAttributes(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, FIELD_FAR thisField, POINTERLIST_HANDLE_FAR StringPoolH) {
    int cpIndex = 0;
    unsigned short attrCount = loadShort(ClassFileH);
    unsigned short attrIndex;

    /* See if the field has any attributes in the class file */
    for (attrIndex = 0; attrIndex < attrCount; attrIndex++) {
        unsigned short attrNameIndex = loadShort(ClassFileH);
        unsigned long  attrLength    = loadCell(ClassFileH);
        PSTR_FAR    attrName     = getUTF8String(StringPoolH, attrNameIndex);

        /* Check if the attribute represents a constant value index */
        if (hstrcmp(attrName.common_ptr_, address_24_of("ConstantValue")) == 0) {
            if (attrLength != 2) {
                PSTR_FAR msg;
                msg.common_ptr_ = address_24_of(KVM_MSG_BAD_CONSTANTVALUE_LENGTH);
                raiseExceptionWithMessage(ClassFormatError, msg);
            }
            if (cpIndex != 0) {
                PSTR_FAR msg;
                msg.common_ptr_ = address_24_of(KVM_MSG_DUPLICATE_CONSTANTVALUE_ATTRIBUTE);
                raiseExceptionWithMessage(ClassFormatError, msg);
            }
            /* Read index to a constant in constant pool */
            cpIndex = loadShort(ClassFileH);
            if (cpIndex == 0) {
                PSTR_FAR msg;
                msg.common_ptr_ = address_24_of(KVM_MSG_BAD_CONSTANT_INDEX);
                raiseExceptionWithMessage(ClassFormatError, msg);
            }
        } else {
            /* Unrecognized attribute; read the bytes to /dev/null */
            skipBytes(ClassFileH, attrLength);
        }
    }

    //thisField->u.offset = cpIndex;
    setWordAt(thisField.common_ptr_ + FIELD_U, cpIndex);
    (void)CurrentClass;
}
//
///*=========================================================================
//* FUNCTION:      verifyFieldFlags()
//* TYPE:          private class file load operation
//* OVERVIEW:      validate field access flags
//* INTERFACE:
//*   parameters:  field access flags, isInterface
//*   returns:     nothing
//*   throws:      ClassFormatError if the field flags are invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void verifyFieldFlags(u2 flags, u2 classFlags) {
    if ((classFlags & ACC_INTERFACE) == 0) {
        /* Class or instance fields */
        int accessFlags = flags & (ACC_PUBLIC | ACC_PRIVATE | ACC_PROTECTED);

        /* Make sure that accessFlags has one of the four legal values, by
        * looking it up in a bit mask */
        if (( (1 << accessFlags) & ((1 << 0) + (1 << ACC_PUBLIC)
            + (1 << ACC_PRIVATE) + (1 << ACC_PROTECTED))) == 0) {
                goto failed;
        }

        if ((flags & (ACC_FINAL | ACC_VOLATILE)) == (ACC_FINAL | ACC_VOLATILE)){
            /* A field can't be both final and volatile */
            goto failed;
        }
    } else {
        /* interface fields */
        if (flags != (ACC_STATIC | ACC_FINAL | ACC_PUBLIC)) {
            goto failed;
        }
    }
    return;

failed:
    {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_FIELD_ACCESS_FLAGS);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }
}
//
//
///*=========================================================================
//* FUNCTION:      loadFields()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the fields (static & instance variables) defined
//*                in a Java class file.
//* INTERFACE:
//*   parameters:  classfile pointer, current class pointer
//*   returns:     <nothing>
//*   throws:      ClassFormatError if any part of the field table
//*                is invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void loadFields(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, POINTERLIST_HANDLE_FAR StringPoolH) {
    u2 fieldCount  = loadShort(ClassFileH);
    int fieldTableSize = SIZEOF_FIELDTABLE(fieldCount);
    FIELDTABLE_FAR fieldTable;
    u2 index;

    int staticPtrCount = 0;
    int staticNonPtrCount = 0;

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose) {
    //        fprintf(stdout, "Loading fields\n");
    //    }
    //#endif /* INCLUDEDEBUGCODE */

    if (fieldCount == 0) {
        return;
    }

    /* Create a new field table */
    //#if USESTATIC
    //    fieldTable = (FIELDTABLE)callocObject(fieldTableSize, GCT_NOPOINTERS);
    //#else
    fieldTable.common_ptr_ = callocPermanentObject(fieldTableSize);
    //#endif

    /* Store the size of the table at the beginning of the table */
    //fieldTable->length = fieldCount;
    setWordAt(fieldTable.common_ptr_ + FIELDTABLE_LENGTH, fieldCount);

    //CurrentClass->fieldTable = fieldTable;
    setDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_FIELDTABLE, fieldTable.common_ptr_);

    for (index = 0; index < fieldCount; index++) {
        u2 accessFlags = loadShort(ClassFileH) & RECOGNIZED_FIELD_FLAGS;
        u2 nameIndex   = loadShort(ClassFileH);
        u2 typeIndex   = loadShort(ClassFileH);
        BOOL isStatic = (accessFlags & ACC_STATIC) != 0;
        FIELD_FAR thisField;

        START_TEMPORARY_ROOTS
            DECLARE_TEMPORARY_ROOT(PSTR_FAR, fieldName, getUTF8String(StringPoolH, nameIndex));
        DECLARE_TEMPORARY_ROOT(PSTR_FAR, signature, getUTF8String(StringPoolH, typeIndex));

        NameTypeKey result;

        verifyFieldFlags(accessFlags, getWordAt(CurrentClass.common_ptr_ + CLASS_ACCESSFLAGS));
        verifyName(fieldName, LegalField);
        verifyFieldType(signature);

        {
            CONST_CHAR_HANDLE_FAR cchf;
            cchf.common_ptr_ = (far_ptr)&fieldName;
            result.nt.nameKey = change_Name_to_Key(cchf, 0, hstrlen(fieldName.common_ptr_));
            cchf.common_ptr_ = (far_ptr)&signature;
            result.nt.typeKey = change_FieldSignature_to_Key(cchf, 0, hstrlen(signature.common_ptr_));
        }

        //        ASSERTING_NO_ALLOCATION
        //thisField = &CurrentClass->fieldTable->fields[index];
        thisField.common_ptr_ = getDWordAt(getDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_FIELDTABLE) + index * SIZEOF_FIELD);

        //#if INCLUDEDEBUGCODE
        //        if (traceclassloadingverbose) {
        //            fprintf(stdout, "Field '%s' loaded\n", fieldName);
        //        }
        //#endif /* INCLUDEDEBUGCODE */

        /* Check if the field is double length, or is a pointer type.
        * If so set the appropriate bit in the word */
        switch (getCharAt(signature.common_ptr_)) {
                   case 'D': case 'J':   accessFlags |= ACC_DOUBLE;   break;
                   case 'L': case '[':   accessFlags |= ACC_POINTER;  break;
        }

        /* Store the corresponding information in the structure */
        //thisField->nameTypeKey = result;
        setDWordAt(thisField.common_ptr_, result.i);
        //thisField->ofClass     = CurrentClass;
        setDWordAt(thisField.common_ptr_ + FIELD_OFCLASS, CurrentClass.common_ptr_);
        //thisField->accessFlags = accessFlags;
        setWordAt(thisField.common_ptr_ + FIELD_ACCESSFLAGS, accessFlags);
        if (isStatic) {
            loadStaticFieldAttributes(ClassFileH, CurrentClass, thisField, StringPoolH);
            if (accessFlags & ACC_POINTER) {
                staticPtrCount++;
            } else {
                staticNonPtrCount += (accessFlags & ACC_DOUBLE) ? 2 : 1;
            }
        } else {
            ignoreAttributes(ClassFileH, StringPoolH);
        }
        //END_ASSERTING_NO_ALLOCATION
        END_TEMPORARY_ROOTS
    }

    /* We now go back and look at each of the static fields again. */
    if (staticPtrCount > 0 || staticNonPtrCount > 0) {
        /* We put all the statics into a POINTERLIST.  We specifically make
        * a POINTERLIST in which the real length is longer than the value
        * put into the length field.  The garbage collector only will look
        * at the first "length" fields.
        * So all the pointer statics go before all the non-pointer statics.
        *
        * As a hack, loadStaticFieldAttributes() has put into the offset
        * field the constant pool entry containing the static's initial
        * value.  The static field should be initialized appropriately
        * once space for it has been allocated
        */

        /* Allocate space for all the pointers and non pointers.
        */
        int staticsSize = SIZEOF_POINTERLIST(staticNonPtrCount+staticPtrCount);
        POINTERLIST_FAR statics;
        /* All the non-pointers go after all the pointers */
        far_ptr nextPtrField;
        far_ptr nextNonPtrField;
        //////////////////////////////////////////////////////////////////////////
        statics.common_ptr_ = callocPermanentObject(staticsSize);
        /* All the non-pointers go after all the pointers */
        nextPtrField = statics.common_ptr_ + POINTERLIST_DATA;
        nextNonPtrField = nextPtrField + staticPtrCount * sizeof(cellOrPointer);


        /* Set the length field so that the GC only looks at the pointers */
        //statics->length = staticPtrCount;
        setWordAt(statics.common_ptr_ + POINTERLIST_LENGTH, staticPtrCount);

        //CurrentClass->staticFields = statics;
        setDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_STATICFIELDS, statics.common_ptr_);

        //        ASSERTING_NO_ALLOCATION
        {
            CONSTANTPOOL_FAR ConstantPool;
            ConstantPool.common_ptr_ = getDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_CONSTPOOL);
            //if (USESTATIC) {
            //    /* Otherwise, this is in permanent memory and won't move */
            //    fieldTable = CurrentClass->fieldTable;
            //}

            FOR_EACH_FIELD(thisField, fieldTable)
                u2 accessFlags = getWordAt(thisField.common_ptr_ + FIELD_ACCESSFLAGS);
            unsigned short cpIndex;
            if (!(accessFlags & ACC_STATIC)) {
                continue;
            }
            cpIndex = getWordAt(thisField.common_ptr_ + FIELD_U);
            if (getWordAt(thisField.common_ptr_ + FIELD_ACCESSFLAGS) & ACC_POINTER) {
                /* The only possible initialization is for a string */
                //thisField->u.staticAddress = nextPtrField;
                setDWordAt(thisField.common_ptr_ + FIELD_U, nextPtrField);
                if (cpIndex != 0) {
                    verifyConstantPoolEntry(CurrentClass, cpIndex, CONSTANT_String);
                    //*(INTERNED_STRING_INSTANCE *)nextPtrField = CP_ENTRY(cpIndex).String;
                    setDWordAt(nextPtrField, getDWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct)));
                }
                nextPtrField += sizeof(cellOrPointer);
            } else {
                //thisField->u.staticAddress = nextNonPtrField;
                setDWordAt(thisField.common_ptr_ + FIELD_U, nextNonPtrField);
                if (cpIndex != 0) {
                    u1 tag;
                    switch(getCharAt(thisField.common_ptr_ + FIELD_NAMETYPEKEY + NTKEY_TYPEKEY)) {
                                case 'B': case 'C': case 'Z': case 'S': case 'I':
                                    tag = CONSTANT_Integer;
                                    break;
                                case 'F':
                                    //#if !IMPLEMENTS_FLOAT
                                    fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
                                    //#endif
                                    tag = CONSTANT_Float;
                                    break;
                                case 'D':
                                    //#if !IMPLEMENTS_FLOAT
                                    fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
                                    //#endif
                                    tag = CONSTANT_Double;
                                    break;
                                case 'J':
                                    tag = CONSTANT_Long;
                                    break;
                                default:
                                    {
                                        PSTR_FAR msg;
                                        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_SIGNATURE);
                                        raiseExceptionWithMessage(ClassFormatError, msg);
                                    }
                    }
                    verifyConstantPoolEntry(CurrentClass, cpIndex, tag);
                    if (accessFlags & ACC_DOUBLE) {
                        /* Initialize a double or long */
                        CONSTANTPOOL_ENTRY_FAR thisEntry;// = &CP_ENTRY(cpIndex);
                        u4 hiBytes, loBytes;
                        thisEntry.common_ptr_ = ConstantPool.common_ptr_ + cpIndex * CONST_ENTRY_SIZE;

                        //hiBytes = (unsigned long)(thisEntry[0].integer);
                        //loBytes = (unsigned long)(thisEntry[1].integer);

                        hiBytes = getDWordAt(thisEntry.common_ptr_);
                        loBytes = getDWordAt(thisEntry.common_ptr_ + CONST_ENTRY_SIZE);

                        SET_LONG_FROM_HALVES(nextNonPtrField, hiBytes, loBytes);
                    } else {
                        //*(cell *)nextNonPtrField = CP_ENTRY(cpIndex).integer;
                        setDWordAt(nextNonPtrField, getDWordAt(ConstantPool.common_ptr_ + cpIndex * CONST_ENTRY_SIZE));
                    }
                }
                nextNonPtrField += sizeof(u4) * ((accessFlags & ACC_DOUBLE) ? 2 : 1);
            }
            END_FOR_EACH_FIELD
        }
        //END_ASSERTING_NO_ALLOCATION
    }

    if (fieldCount >= 2) {
        //if (USESTATIC) {
        //    /* Otherwise, this is in permanent memory and won't move */
        //    fieldTable = CurrentClass->fieldTable;
        //}
        //ASSERTING_NO_ALLOCATION
        /* Check to see if there are two fields with the same name/type */
        FIELD_FAR firstField;
        FIELD_FAR lastField;
        FIELD_FAR outer, inner;
        firstField.common_ptr_ = fieldTable.common_ptr_ + FIELDTABLE_FIELDS;
        lastField.common_ptr_ = firstField.common_ptr_ + (fieldCount - 1) * SIZEOF_FIELD;

        for (outer = firstField; outer.common_ptr_ < lastField.common_ptr_; outer.common_ptr_ += SIZEOF_FIELD) {
            for (inner.common_ptr_ = outer.common_ptr_ + SIZEOF_FIELD; inner.common_ptr_ <= lastField.common_ptr_; inner.common_ptr_ += SIZEOF_FIELD) {
                //                if (outer->nameTypeKey.i == inner->nameTypeKey.i) {
                if (getDWordAt(outer.common_ptr_ + FIELD_NAMETYPEKEY) == getDWordAt(inner.common_ptr_ + FIELD_NAMETYPEKEY)) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_DUPLICATE_FIELD_FOUND);
                    raiseExceptionWithMessage(ClassFormatError, msg);
                }
            }
        }
        //END_ASSERTING_NO_ALLOCATION
    }

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose)
    //        fprintf(stdout, "Fields loaded ok\n");
    //#endif /* INCLUDEDEBUGCODE */

}
//
//
//
//
///*=========================================================================
//* FUNCTION:      verifyClassFlags()
//* TYPE:          private class file load operation
//* OVERVIEW:      validate class access flags
//* INTERFACE:
//*   parameters:  class access flags
//*   returns:     nothing
//*   throws:      ClassFormatError if the flags are invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void verifyClassFlags(u2 flags) {
    if (flags & ACC_INTERFACE) {
        if ((flags & ACC_ABSTRACT) == 0)
            goto failed;
        if (flags & ACC_FINAL)
            goto failed;
    } else {
        if ((flags & ACC_FINAL) && (flags & ACC_ABSTRACT))
            goto failed;
    }
    return;

failed:
    {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_CLASS_ACCESS_FLAGS);
        raiseExceptionWithMessage(ClassFormatError, msg);

    }
}
//
//
///*=========================================================================
//* FUNCTION:      loadClassInfo()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the access flag, thisClass and superClass
//*                parts of a Java class file.
//* INTERFACE:
//*   parameters:  classfile pointer, current constant pool pointer
//*   returns:     Pointer to class runtime structure
//*   throws:      ClassFormatError if the class flags are invalid,
//*                the thisClass or superClass indices are invalid,
//*                or if the superClass is invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*                NoClassDefFoundError if thisClass != CurrentClass
//*=======================================================================*/
void loadClassInfo(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass)
{
    INSTANCE_CLASS_FAR thisClass;
    INSTANCE_CLASS_FAR superClass;
    u2 accessFlags = loadShort(ClassFileH) & RECOGNIZED_CLASS_FLAGS;
    CONSTANTPOOL_FAR ConstantPool;
    ConstantPool.common_ptr_ = getDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_CONSTPOOL);

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose) {
    //        fprintf(stdout, "Loading class info\n");
    //    }
    //#endif /* INCLUDEDEBUGCODE */

    verifyClassFlags(accessFlags);

    {
        unsigned short thisClassIndex = loadShort(ClassFileH);
        verifyConstantPoolEntry(CurrentClass, thisClassIndex, CONSTANT_Class);
        //thisClass = (INSTANCE_CLASS)CP_ENTRY(thisClassIndex).clazz;
        thisClass.common_ptr_ = getDWordAt(ConstantPool.common_ptr_ + thisClassIndex * sizeof(union constantPoolEntryStruct));

        if (CurrentClass.common_ptr_ != thisClass.common_ptr_) {
            /*
            * JVM Spec 5.3.5:
            *
            *   Otherwise, if the purported representation does not actually
            *   represent a class named N, loading throws an instance of
            *   NoClassDefFoundError or an instance of one of its
            *   subclasses.
            */
            raiseException(NoClassDefFoundError);
        }
    }
    {
        unsigned short superClassIndex = loadShort(ClassFileH);
        if (superClassIndex == 0) {
            superClass.common_ptr_ = 0;
        } else {
            verifyConstantPoolEntry(CurrentClass, superClassIndex, CONSTANT_Class);
            //superClass = (INSTANCE_CLASS)CP_ENTRY(superClassIndex).clazz;
            superClass.common_ptr_ = getDWordAt(ConstantPool.common_ptr_ + superClassIndex * sizeof(union constantPoolEntryStruct));
        }
    }

    //CurrentClass->superClass = superClass;
    setDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_SUPERCLASS, superClass.common_ptr_);
    //CurrentClass->clazz.accessFlags = accessFlags;
    setWordAt(CurrentClass.common_ptr_ + CLASS_ACCESSFLAGS, accessFlags);

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose)
    //        fprintf(stdout, "Class info loaded ok\n");
    //#endif /* INCLUDEDEBUGCODE */

}
//
///*=========================================================================
//* FUNCTION:      loadStackMaps()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the stack maps associated
//*                with each method in a class file.
//* INTERFACE:
//*   parameters:  classfile pointer, method pointer, constant pool
//*   returns:     number of characters read from the class file
//*   throws:      ClassFormatError if any part of the stack maps
//*                is invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
u4 loadStackMaps(FILEPOINTER_HANDLE_FAR ClassFileH, METHOD_FAR* thisMethodH) {
    u4 bytesRead;
    //INSTANCE_CLASS_FAR CurrentClass = unhand(thisMethodH)->ofClass;
    INSTANCE_CLASS_FAR CurrentClass;
    CurrentClass.common_ptr_ = getDWordAt(thisMethodH->common_ptr_ + METHOD_OFCLASS);
    START_TEMPORARY_ROOTS
        unsigned short nStackMaps = loadShort(ClassFileH);
    POINTERLIST_FAR pl;
    pl.common_ptr_ = callocObject(SIZEOF_POINTERLIST(2 * nStackMaps), GCT_POINTERLIST);
    {
        DECLARE_TEMPORARY_ROOT(POINTERLIST_FAR, stackMaps, pl);
        METHOD_FAR thisMethod = unhand(thisMethodH); /* Very volatile */
        u2 tempSize = getWordAt(thisMethod.common_ptr_ + METHOD_U + JAVA_MAXSTACK) + getWordAt(thisMethod.common_ptr_ + METHOD_FRAMESIZE) + 2;
        WORDS_FAR wf;
        wf.common_ptr_ = mallocBytes(sizeof(u2) * tempSize);
        {
            DECLARE_TEMPORARY_ROOT(WORDS_FAR, stackMap, wf);
            u2 stackMapIndex;

            //stackMaps->length = nStackMaps;
            setWordAt(stackMaps.common_ptr_ + POINTERLIST_LENGTH, nStackMaps);
            //unhand(thisMethodH)->u.java.stackMaps.verifierMap = stackMaps;
            setDWordAt(thisMethodH->common_ptr_ + METHOD_U + JAVA_STACKMAPS, stackMaps.common_ptr_);

            bytesRead = 2;

            for (stackMapIndex = 0; stackMapIndex < nStackMaps; stackMapIndex++) {
                u2 i, index;
                /* Any allocation happens at the end, so we have to dereference this
                * at least once through each loop.
                */
                thisMethod = unhand(thisMethodH);
                /* Read in the offset */
                //stackMaps->data[stackMapIndex + nStackMaps].cell =
                //    loadShort(ClassFileH);
                setDWordAt(stackMaps.common_ptr_ + POINTERLIST_DATA + (stackMapIndex + nStackMaps) * sizeof(cellOrPointer), loadShort(ClassFileH));
                bytesRead += 2;
                for (index = 0, i = 0 ; i < 2; i++) {
                    u2 j;
                    u2 size = loadShort(ClassFileH);
                    u2 size_delta = 0;
                    u2 size_index = index++;
                    u2 maxSize = (i == 0 ? getWordAt(thisMethod.common_ptr_ + METHOD_FRAMESIZE) : getWordAt(thisMethod.common_ptr_ + METHOD_U + JAVA_MAXSTACK));
                    bytesRead += 2;
                    for (j = 0; j < size; j++) {
                        u1 stackType = loadByte(ClassFileH);
                        bytesRead += 1;

                        /* We are reading the j-th element of the stack map.
                        * This corresponds to the value in the j + size_delta'th
                        * local register or stack location
                        */
                        if (j + size_delta >= maxSize) {
                            PSTR_FAR msg;
                            msg.common_ptr_ = address_24_of(KVM_MSG_BAD_STACKMAP);
                            raiseExceptionWithMessage(ClassFormatError, msg);
                        } else if (stackType == ITEM_NewObject) {
                            u2 instr = loadShort(ClassFileH);
                            bytesRead += 2;
                            if (instr >= getWordAt(thisMethod.common_ptr_ + METHOD_U + JAVA_CODELENGTH)) {
                                PSTR_FAR msg;
                                msg.common_ptr_ = address_24_of(KVM_MSG_BAD_NEWOBJECT);
                                raiseExceptionWithMessage(ClassFormatError, msg);
                            }

                            //stackMap[index++] = ENCODE_NEWOBJECT(instr);
                            setWordAt(stackMap.common_ptr_ + (index++) * sizeof(u2), ENCODE_NEWOBJECT(instr));

                        } else if (stackType < ITEM_Object) {
                            //stackMap[index++] = stackType;
                            setWordAt(stackMap.common_ptr_ + (index++) * sizeof(u2), stackType);
                            if (stackType == ITEM_Long || stackType == ITEM_Double){
                                if (j + size_delta + 1 >= maxSize) {
                                    PSTR_FAR msg;
                                    msg.common_ptr_ = address_24_of(KVM_MSG_BAD_STACKMAP);
                                    raiseExceptionWithMessage(ClassFormatError, msg);
                                }
                                //stackMap[index++] = (stackType == ITEM_Long)
                                //    ? ITEM_Long_2
                                //    : ITEM_Double_2;
                                setWordAt(stackMap.common_ptr_ + (index++) * sizeof(u2), (stackType == ITEM_Long)
                                    ? ITEM_Long_2
                                    : ITEM_Double_2);

                                size_delta++;
                            }
                        } else if (stackType == ITEM_Object) {
                            u2 classIndex = loadShort(ClassFileH);
                            CONSTANTPOOL_FAR ConstantPool;// = CurrentClass->constPool;
                            CLASS_FAR clazz;
                            ConstantPool.common_ptr_ = getDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_CONSTPOOL);
                            bytesRead += 2;
                            verifyConstantPoolEntry(CurrentClass, classIndex, CONSTANT_Class);
                            //clazz = CP_ENTRY(classIndex).clazz;
                            clazz.common_ptr_ = getDWordAt(ConstantPool.common_ptr_ + classIndex * sizeof(union constantPoolEntryStruct));
                            //stackMap[index++] = clazz->key;
                            setWordAt(stackMap.common_ptr_ + (index++) * sizeof(u2), getWordAt(clazz.common_ptr_ + CLASS_KEY));
                        } else {
                            PSTR_FAR msg;
                            msg.common_ptr_ = address_24_of(KVM_MSG_BAD_STACKMAP);
                            raiseExceptionWithMessage(ClassFormatError, msg);
                        }
                    }
                    //stackMap[size_index] = size + size_delta;
                    setWordAt(stackMap.common_ptr_ + size_index * sizeof(u2), size + size_delta);
                }

                /* We suspect that there will be a lot of duplication, so it's worth
                * it to check and see if we already have this identical string */
                for (i = 0; ; i++) {
                    if (i == stackMapIndex) {
                        /* We've reached the end, and no duplicate found */
                        far_ptr temp = mallocBytes(index * sizeof(u2));
                        hmemcpy(temp, stackMap.common_ptr_, index * sizeof(u2));
                        //stackMaps->data[stackMapIndex].cellp = (cell*)temp;
                        setDWordAt(stackMaps.common_ptr_ + POINTERLIST_DATA + stackMapIndex * sizeof(cellOrPointer), temp);
                        break;
                    } else {
                        //unsigned short *tempMap =
                        //    (unsigned short *)stackMaps->data[i].cellp;

                        WORDS_FAR tempMap;
                        tempMap.common_ptr_ = stackMaps.common_ptr_ + POINTERLIST_DATA + i * sizeof(cellOrPointer);
                        {
                            /* get length of stored map entry 0 */
                            u2 tempLen = getWordAt(tempMap.common_ptr_);
                            /* and length of current map being created */
                            u2 mapLen = getWordAt(stackMap.common_ptr_);
                            /* add in entry 1 */
                            tempLen += getWordAt(tempMap.common_ptr_ + (tempLen + 1) * sizeof(u2)) + 2;
                            mapLen += getWordAt(stackMap.common_ptr_ + (mapLen + 1) * sizeof(u2)) + 2;
                            /* if lens not the same, not a duplicate */
                            if (mapLen == tempLen &&
                                hmemcmp(stackMap.common_ptr_, tempMap.common_ptr_,
                                mapLen * sizeof(u2)) == 0) {
                                    /* We have found a duplicate */
                                    //stackMaps->data[stackMapIndex].cellp = (cell*)tempMap;
                                    setDWordAt(stackMaps.common_ptr_ + POINTERLIST_DATA + stackMapIndex * sizeof(cellOrPointer), tempMap.common_ptr_);
                                    break;
                            }
                        }
                    }
                }
            }
        }
    }
    END_TEMPORARY_ROOTS
        return bytesRead;
}
//
//
///*=========================================================================
//* FUNCTION:      loadInterfaces()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the interface part of a Java class file.
//* INTERFACE:
//*   parameters:  classfile pointer, current class pointer
//*   returns:     <nothing>
//*   throws:      ClassFormatError if any of the constant pool indices
//*                in the interface table are invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void loadInterfaces(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass) {
    u2 interfaceCount = loadShort(ClassFileH);
    u2 byteSize = (interfaceCount + 1) * sizeof(u2);
    u2 ifIndex;

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose) {
    //        fprintf(stdout, "Loading interfaces\n");
    //    }
    //#endif /* INCLUDEDEBUGCODE */

    if (interfaceCount == 0) {
        return;
    }

    //if (USESTATIC) {
    //    CurrentClass->ifaceTable = (unsigned short *)mallocBytes(byteSize);
    //} else {
    //CurrentClass->ifaceTable =
    //    (unsigned short*)callocPermanentObject(cellSize);
    {
        far_ptr interfaces = callocPermanentObject(byteSize);
        setDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_IFACETABLE, interfaces);
        //}

        /* Store length in the first slot */
        //CurrentClass->ifaceTable[0] = interfaceCount;
        setWordAt(interfaces, interfaceCount);

        for (ifIndex = 1; ifIndex <= interfaceCount; ifIndex++) {
            const u2 cpIndex = loadShort(ClassFileH);
            verifyConstantPoolEntry(CurrentClass, cpIndex, CONSTANT_Class);
            //CurrentClass->ifaceTable[ifIndex] = cpIndex;
            setWordAt(interfaces + ifIndex * sizeof(u2), cpIndex);
        }

    }

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose) {
    //        fprintf(stdout, "Interfaces loaded ok\n");
    //    }
    //#endif /* INCLUDEDEBUGCODE */

}
//
//
//
///*=========================================================================
//* FUNCTION:      verifyUTF8String()
//* TYPE:          private class file load operation
//* OVERVIEW:      validate a UTF8 string
//* INTERFACE:
//*   parameters:  pointer to UTF8 string, length
//*   returns:     nothing
//*   throws:      ClassFormatError if the string is invalid
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void verifyUTF8String(far_ptr bytes, unsigned short length)
{
    unsigned int i;
    for (i=0; i<length; i++) {
        unsigned int c = ((unsigned char *)bytes)[i];
        if (c == 0) /* no embedded zeros */
            goto failed;
        if (c < 128)
            continue;
        switch (c >> 4) {
        default:
            break;

        case 0x8: case 0x9: case 0xA: case 0xB: case 0xF:
            goto failed;

        case 0xC: case 0xD:
            /* 110xxxxx  10xxxxxx */
            i++;
            if (i >= length)
                goto failed;
            if ((getCharAt(bytes + i) & 0xC0) == 0x80)
                break;
            goto failed;

        case 0xE:
            /* 1110xxxx 10xxxxxx 10xxxxxx */
            i += 2;
            if (i >= length)
                goto failed;
            if (((getCharAt(bytes + i - 1) & 0xC0) == 0x80) &&
                ((getCharAt(bytes + i) & 0xC0) == 0x80))
                break;
            goto failed;
        } /* end of switch */
    }
    return;

failed:
    {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_UTF8_STRING);
        raiseExceptionWithMessage(ClassFormatError, msg);

    }
}
//
//
///*=========================================================================
//* FUNCTION:      loadVersionInfo()
//* TYPE:          private class file load operation
//* OVERVIEW:      Load the first few bytes of a Java class file,
//*                checking the file type and version information.
//* INTERFACE:
//*   parameters:  classfile pointer
//*   returns:     <nothing>
//*   throws:      ClassFormatError if this is not a Java class file
//*                (this error class is not supported by CLDC 1.0 or 1.1)
//*=======================================================================*/
void loadVersionInfo(FILEPOINTER_HANDLE_FAR ClassFileH)
{
    u4 magic;
    u2 majorVersion;
    u2 minorVersion;

    //#if INCLUDEDEBUGCODE
    //    if (traceclassloadingverbose) {
    //        fprintf(stdout, "Loading version information\n");
    //    }
    //#endif /* INCLUDEDEBUGCODE */

    magic = loadCell(ClassFileH);
    if (magic != 0xCAFEBABE) {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_MAGIC_VALUE);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }
    /* check version information */
    minorVersion = loadShort(ClassFileH);
    majorVersion = loadShort(ClassFileH);
    if ((majorVersion < JAVA_MIN_SUPPORTED_VERSION) ||
        (majorVersion > JAVA_MAX_SUPPORTED_VERSION)) {
            PSTR_FAR msg;
            msg.common_ptr_ = address_24_of(KVM_MSG_BAD_VERSION_INFO);
            raiseExceptionWithMessage(ClassFormatError, msg);
    }
}
