/* project : javaonzx
   file    : locader.c
   purpose : class loader
   author  : valker
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <intrz80.h>
#include "jvm_types.h"
#include "loader.h"
#include "root_code.h"
#include "class.h"
#include "seh.h"
#include "messages.h"
#include "frame.h"
#include "pool.h"
#include "fields.h"


BOOL loadedReflectively;

static void loadRawClass(INSTANCE_CLASS_FAR CurrentClass, BOOL fatalErrorIfFail);
static INSTANCE_CLASS_FAR findSuperMostUnlinked(INSTANCE_CLASS_FAR clazz);
static POINTERLIST_FAR loadConstantPool(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
static void loadVersionInfo(FILEPOINTER_HANDLE_FAR ClassFileH);
static void loadClassInfo(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
static void loadInterfaces(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass);
static void loadFields(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, POINTERLIST_HANDLE_FAR StringPoolH);
static void loadMethods(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass, POINTERLIST_HANDLE_FAR StringPoolH);
static void ignoreAttributes(FILEPOINTER_HANDLE_FAR ClassFile, POINTERLIST_HANDLE_FAR StringPool);
static void verifyUTF8String(far_ptr bytes, unsigned short length);
static PSTR_FAR getUTF8String(POINTERLIST_HANDLE_FAR StringPoolH, u2 index);
static void verifyName(PSTR_FAR name, enum validName_type type);
static u2 verifyMethodType(PSTR_FAR name, PSTR_FAR signature);
static void verifyFieldType(PSTR_FAR type);
static PSTR_FAR skipOverFieldType(PSTR_FAR string, BOOL void_okay, u2 length);
static PSTR_FAR skipOverFieldName(PSTR_FAR string, BOOL slash_okay, u2 length);

u2 readClassStatus(INSTANCE_CLASS_FAR clazz) {
    return getWordAt(clazz.common_ptr_ + INSTANCE_CLASS_STATUS);
}

/*=========================================================================
* FUNCTION:      loadClassfile()
* TYPE:          constructor (kind of)
* OVERVIEW:      Load and link the given Java class from a classfile in
*                the system. This function iteratively traverses the
*                superclass hierarchy but recursively traverses the
*                interface hierarchy.
* INTERFACE:
*   parameters:  class structure.  Only the name field is filled in.  All
*                other fields must be NULL and the class status must be
*                CLASS_RAW.
*   returns:     <nothing>
*   throws:      NoClassDefFoundError or ClassNotFoundException,
*                depending on who called this function.
*                ClassCircularityError, ClassFormatError, VerifyError
*                IncompatibleClassChangeError
*=======================================================================*/
void loadClassfile(INSTANCE_CLASS_FAR InitiatingClass, BOOL fatalErrorIfFail) {
    /*
    * This must be volatile so that it's value is kept when an exception
    * occurs in the TRY block.
    */
    volatile INSTANCE_CLASS_FAR clazz = InitiatingClass;

    /*
    * The class status must be CLASS_RAW.
    */
    {
        if (readClassStatus(clazz) != CLASS_RAW) {
            fatalVMError(KVM_MSG_EXPECTED_CLASS_STATUS_OF_CLASS_RAW);
        }
    }

    TRY {

        /*
        * Iteratively load the raw class bytes of the class and its raw
        * superclasses.
        */
        while (clazz.common_ptr_ != 0 && readClassStatus(clazz) == CLASS_RAW) {
            //clazz->status = CLASS_LOADING;
            setWordAt(clazz.common_ptr_ + INSTANCE_CLASS_STATUS, CLASS_LOADING);
            loadRawClass(clazz, fatalErrorIfFail);
            if (!fatalErrorIfFail && readClassStatus(clazz) == CLASS_ERROR) {
                return;
            }
            //clazz->status = CLASS_LOADED;
            setWordAt(clazz.common_ptr_ + INSTANCE_CLASS_STATUS, CLASS_LOADED);

            /*
            * Now go up to the superclass.
            */
            clazz.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS);

            if (clazz.common_ptr_ != 0) {
                /*
                * The class will be in CLASS_ERROR if an error occurred
                * during its initialization.
                */
                const u2 status = readClassStatus(clazz);
                if (status == CLASS_ERROR) {
                    raiseException(NoClassDefFoundError);
                }
                /*
                * If a class is in CLASS_LOADED state, then this is
                * equivalent to a recursive call to loadClassfile for the
                * class and as such indicates as ClassCircularityError.
                */
                else if (status == CLASS_LOADED) {
                    raiseException(ClassCircularityError);
                }
                /*
                * Any other state is a VM error.
                */
                else if (status != CLASS_RAW && status < CLASS_LINKED) {
                    fatalVMError(KVM_MSG_EXPECTED_CLASS_STATUS_OF_CLASS_RAW_OR_CLASS_LINKED);
                }
            }
        }

        /*
        * Iteratively link the class and its unlinked superclass.
        */
        while ((clazz = findSuperMostUnlinked(InitiatingClass)).common_ptr_ != 0) {

//#if INCLUDEDEBUGCODE
//            if (traceclassloading || traceclassloadingverbose) {
//                fprintf(stdout, "Linking class: '%s'\n",
//                    getClassName((CLASS)clazz));
//            }
//#endif

            /*
            * Link the superclass.
            */
            if (getWordAt(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS) == 0) {
                /*
                * This must be java.lang.Object.
                */
                if (clazz.common_ptr_ != JavaLangObject.common_ptr_) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(&KVM_MSG_BAD_SUPERCLASS);
                    raiseExceptionWithMessage(ClassFormatError, msg);
                }
                /*
                * Treat 'java.lang.Object' specially (it has no superclass
                */
                //clazz->instSize = 0;
                setWordAt(clazz.common_ptr_ + INSTANCE_CLASS_INSTSIZE, 0);
            }
            else {
                INSTANCE_CLASS_FAR superClass;
                superClass.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS);
                /*
                * Cannot inherit from an array class.
                */
                {
                    CLASS_FAR cf;
                    cf.common_ptr_ = superClass.common_ptr_;
                    if (IS_ARRAY_CLASS(cf)) {
                        PSTR_FAR msg;
                        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_SUPERCLASS);
                        raiseExceptionWithMessage(ClassFormatError, msg);
                    }
                }

                /*
                * The superclass cannot be an interface. From the
                * JVM Spec section 5.3.5:
                *
                *   If the class of interface named as the direct
                *   superclass of C is in fact an interface, loading
                *   throws an IncompatibleClassChangeError.
                */
                if (/*superClass->clazz.accessFlags*/getWordAt(superClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_INTERFACE) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_CLASS_EXTENDS_INTERFACE);
                    raiseExceptionWithMessage(IncompatibleClassChangeError, msg);
                }
                /*
                * The superclass cannot be final.
                * Inheriting from a final class is a VerifyError according
                * to J2SE JVM behaviour. There is no explicit
                * documentation in the JVM Spec.
                */
                if (/*superClass->clazz.accessFlags*/getWordAt(superClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_FINAL) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_CLASS_EXTENDS_FINAL_CLASS);
                    raiseExceptionWithMessage(VerifyError, msg);
                }

                /*
                * The current class must have access to its super class
                */
                {
                    CLASS_FAR cf;
                    cf.common_ptr_ = superClass.common_ptr_;
                    verifyClassAccess(cf, clazz);
                }

                /*
                * If this is an interface class, its superclass must be
                * java.lang.Object.
                */
                if (superClass.common_ptr_ != JavaLangObject.common_ptr_ &&
                    (/*clazz->clazz.accessFlags*/getWordAt(superClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_INTERFACE)) {
                        PSTR_FAR msg;
                        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_SUPERCLASS);
                        raiseExceptionWithMessage(ClassFormatError, msg);
                }

                /*
                * Compute instance size and instance field offset.
                * Make the instance size of the new class "inherit"
                * the instance size of the superclass
                */
                //clazz->instSize = superClass->instSize;
                setWordAt(clazz.common_ptr_, getWordAt(superClass.common_ptr_));
            }

            /*
            * Load and link super-interface(s).
            */
            if (getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_IFACETABLE) != 0) {
                PWORD_FAR ifaceTable;
                u2 ifIndex;
                u2 ifaceCount = getWordAt(ifaceTable.common_ptr_);
                ifaceTable.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_IFACETABLE);
                for (ifIndex = 1; ifIndex <= ifaceCount; ifIndex++) {
                    //INSTANCE_CLASS ifaceClass = (INSTANCE_CLASS)
                    //    clazz->constPool->entries
                    //    [clazz->ifaceTable[ifIndex]].clazz;
                    INSTANCE_CLASS_FAR ifaceClass;
                    CONSTANTPOOL_FAR cPool;
                    u2 ifIndexInPool = getWordAt(ifaceTable.common_ptr_ + ifIndex);
                    cPool.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_CONSTPOOL);
                    ifaceClass.common_ptr_ = getDWordAt(cPool.common_ptr_ + ifIndexInPool * sizeof(union constantPoolEntryStruct));

                    /*
                    * Super-interface cannot be an array class.
                    */
                    {
                        CLASS_FAR cf;
                        cf.common_ptr_ = ifaceClass.common_ptr_;
                        if (IS_ARRAY_CLASS(cf)) {
                            PSTR_FAR msg;
                            msg.common_ptr_ = address_24_of(KVM_MSG_CLASS_IMPLEMENTS_ARRAY_CLASS);
                            raiseExceptionWithMessage(ClassFormatError, msg);
                        }

                    }

                    /*
                    * The class will be in CLASS_ERROR if an error occurred
                    * during its initialization.
                    */
                    {
                        const u2 status = readClassStatus(ifaceClass);
                        if (status == CLASS_ERROR) {
                            raiseException(NoClassDefFoundError);
                        }
                        /*
                        * If the interface is in CLASS_LOADED state, then this is
                        * a recursive call to loadClassfile for the
                        * interface and as such indicates as ClassCircularityError.
                        */
                        else if (status == CLASS_LOADED) {
                            raiseException(ClassCircularityError);
                        }
                        else if (status == CLASS_RAW) {
                            loadClassfile(ifaceClass, TRUE);
                        }
                        else if (status < CLASS_LINKED) {
                            fatalVMError(KVM_MSG_EXPECTED_CLASS_STATUS_GREATER_THAN_EQUAL_TO_CLASS_LINKED);
                        }
                    }

                    /* JVM Spec 5.3.5 */
                    if ((getWordAt(ifaceClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_INTERFACE) == 0) {
                        PSTR_FAR msg;
                        msg.common_ptr_ = address_24_of(KVM_MSG_CLASS_IMPLEMENTS_NON_INTERFACE);
                        raiseExceptionWithMessage(IncompatibleClassChangeError, msg);
                    }

                    /*
                    * Ensure that the current class has access to the
                    * interface class
                    */
                    {
                        CLASS_FAR cf;
                        cf.common_ptr_ = ifaceClass.common_ptr_;
                        verifyClassAccess(cf, clazz);
                    }
                }
            }

            {
                FIELDTABLE_FAR ft;
                ft.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_FIELDTABLE);
                FOR_EACH_FIELD(thisField, ft)
                    u4 accessFlags = getDWordAt(thisField.common_ptr_ + FIELD_ACCESSFLAGS);
                    if ((accessFlags & ACC_STATIC) == 0) {
                        //thisField->u.offset = clazz->instSize;
                        u2 size = getWordAt(clazz.common_ptr_ + INSTANCE_CLASS_INSTSIZE);
                        setWordAt(thisField.common_ptr_ + FIELD_U, size);
                        //clazz->instSize += (accessFlags & ACC_DOUBLE) ? 2 : 1;
                        size += (accessFlags & ACC_DOUBLE) ? 2 : 1;
                        setWordAt(clazz.common_ptr_ + INSTANCE_CLASS_INSTSIZE, size);
                    }
                END_FOR_EACH_FIELD
            }

                /* Move parts of the class to static memory */

                /* **DANGER**
                * While moveClassFieldsToStatic is running, things are in a
                * very strange state as far as GC is concerned.  It is important
                * that this function do no allocation, and that we get rid
                * of the temporary roots registered above before any other
                * allocation is done.
                */
                //moveClassFieldsToStatic(clazz);

            //clazz->status = CLASS_LINKED;
            setWordAt(clazz.common_ptr_ + INSTANCE_CLASS_STATUS, CLASS_LINKED);

//#if ENABLE_JAVA_DEBUGGER
//            if (vmDebugReady) {
//                CEModPtr cep = GetCEModifier();
//                cep->loc.classID = GET_CLASS_DEBUGGERID(&clazz->clazz);
//                cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
//                cep->eventKind = JDWP_EventKind_CLASS_PREPARE;
//                insertDebugEvent(cep);
//            }
//#endif
//
//#if INCLUDEDEBUGCODE
//            if (traceclassloading || traceclassloadingverbose) {
//                fprintf(stdout, "Class linked ok\n");
//            }
//#endif /* INCLUDEDEBUGCODE */
        }

    } CATCH(e) {
        INSTANCE_CLASS_FAR c = InitiatingClass;
        /*
        * Prepend the name of the class to the error message (if any)
        * of the exception if this is the initiating class.
        */
        {
            CLASS_FAR cf;
            cf.common_ptr_ = InitiatingClass.common_ptr_;
            sprintf(str_buffer,"%s", getClassName(cf));
            {
                STRING_INSTANCE_FAR message;
                message.common_ptr_ = getDWordAt(e.common_ptr_ + THROWABLE_MESSAGE);
                if (message.common_ptr_ != 0) {
                    int length = strlen(str_buffer);
                    char buffer[STRINGBUFFERSIZE];
                    getStringContentsSafely(message, buffer, STRINGBUFFERSIZE - length);

                    {
                        PSTR_FAR pf;
                        pf.common_ptr_ = (far_ptr)buffer;
                        if (hstrcmp(getClassName(cf).common_ptr_, pf.common_ptr_)) {
                            sprintf(str_buffer + length, ": %s ", buffer);
                        }
                    }
                }
                {
                    PSTR_FAR pf;
                    pf.common_ptr_ = (far_ptr)str_buffer;
                    message = instantiateString(pf,strlen(str_buffer));
                }
                setDWordAt(e.common_ptr_ + THROWABLE_MESSAGE, message.common_ptr_);
                /*
                * Errors occuring during classfile loading are "transient" errors.
                * That is, their cause is temporal in nature and may not occur
                * if a different classfile is submitted at a later point.
                */
                do {
                    revertToRawClass(c);
                    //c = c->superClass;
                    c.common_ptr_ = getDWordAt(c.common_ptr_ + INSTANCE_CLASS_SUPERCLASS);
                } while (c.common_ptr_ != 0 
                    && c.common_ptr_ != clazz.common_ptr_ 
                    && c.common_ptr_ != InitiatingClass.common_ptr_);
            }
        }
        THROW(e);
    } END_CATCH
}


/*=========================================================================
* FUNCTION:      loadRawClass()
* TYPE:          constructor (kind of)
* OVERVIEW:      Find and load the binary form of the given Java class
*                file. This function is *not* recursive.
* INTERFACE:
*   parameters:  class structure.  Only the name field is filled in.  All
*                other fields must be NULL and the class status must be
*                CLASS_LOADING. The boolean variable fatalErrorIfFail should
*                always be set to TRUE unless a fatal error is not desired while
*                loading a class.
*   returns:     pointer to runtime class structure or NIL if not found
*   throws:      NoClassDefFoundError or ClassNotFoundException,
*                depending on who called this function.
*=======================================================================*/

static void loadRawClass(INSTANCE_CLASS_FAR CurrentClass, BOOL fatalErrorIfFail)
{
    START_TEMPORARY_ROOTS
        /* The UTF8 strings in the constant pool are put into a temporary
        * (directly indexable) list of strings that is discarded after
        * class loading. Only non-UTF8 entries are referenced after a
        * class has been loaded. The references to the UTF8 strings
        * are converted to UStrings.
        */
        POINTERLIST_FAR StringPool;
        FILEPOINTER_FAR fp = openClassfile(CurrentClass);
        DECLARE_TEMPORARY_ROOT(FILEPOINTER_FAR, ClassFile, fp);
        i1 ch;

//#if INCLUDEDEBUGCODE
//    if (traceclassloading || traceclassloadingverbose) {
//        Log->loadClass(getClassName((CLASS)CurrentClass));
//    }
//#endif
    /* If the requested class is not found, we will throw the
    * appropriate exception or error, and include the class
    * name as the message.
    */
    if (ClassFile.common_ptr_ == 0) {
        CLASS_FAR cf;
        cf.common_ptr_ = CurrentClass.common_ptr_;
        {
            DECLARE_TEMPORARY_ROOT(PSTR_FAR, className, getClassName(cf));
            PSTR_FAR exceptionName;

            if (fatalErrorIfFail) {
                //CurrentClass->status = CLASS_RAW;
                setWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_STATUS, CLASS_RAW);

                if (loadedReflectively) {
                    exceptionName = ClassNotFoundException;
                } else {
                    exceptionName = NoClassDefFoundError;
                }
                loadedReflectively = FALSE;

                if (className.common_ptr_) {
                    raiseExceptionWithMessage(exceptionName, className);
                } else {
                    raiseException(exceptionName);
                }
            } else {
                //CurrentClass->status = CLASS_ERROR;
                setWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_STATUS, CLASS_ERROR);
            }
        }
    } else {

//#if ROMIZING
//        UString uPackageName = CurrentClass->clazz.packageName;
//        if (uPackageName != NULL) {
//            char *name = UStringInfo(uPackageName);
//            if (IS_RESTRICTED_PACKAGE_NAME(name)) {
//                raiseExceptionWithMessage(NoClassDefFoundError,
//                    KVM_MSG_CREATING_CLASS_IN_SYSTEM_PACKAGE);
//            }
//        }
//#endif

        /* This flag is true only when class loading is performed */
        /* from Class.forName() native method. Only the initiating */
        /* class (the one being loaded via Class.forName() should */
        /* cause a different exception to be thrown */
        loadedReflectively = FALSE;


        {
            FILEPOINTER_HANDLE_FAR fphf;
            fphf.common_ptr_ = (far_ptr)&ClassFile;
            {
                /* Load version info and magic value */
                loadVersionInfo(fphf);

                {
                    /* Load and create constant pool */
                    POINTERLIST_FAR cp = loadConstantPool(fphf, CurrentClass);
                    IS_TEMPORARY_ROOT(StringPool, cp);
                }

                /* Load class identification information */
                loadClassInfo(fphf, CurrentClass);

                /* Load interface pointers */
                loadInterfaces(fphf, CurrentClass);

                {
                    POINTERLIST_HANDLE_FAR plhf;
                    plhf.common_ptr_ = (far_ptr)&StringPool;
                    /* Load field information */
                    loadFields(fphf, CurrentClass, plhf);

                    /* Load method information */
                    loadMethods(fphf, CurrentClass, plhf);

                    /* Load the possible extra attributes (e.g., debug info) */
                    ignoreAttributes(fphf, plhf);
                }

                ch = loadByteNoEOFCheck(fphf);
                if (ch != -1) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_CLASSFILE_SIZE_DOES_NOT_MATCH);
                    raiseExceptionWithMessage(ClassFormatError, msg);
                }

                /* Ensure that EOF has been reached successfully and close file */
                closeClassfile(fphf);

            }
        }


        /* Make the class an instance of class 'java.lang.Class' */
        //CurrentClass->clazz.ofClass = JavaLangClass;
        setDWordAt(CurrentClass.common_ptr_, JavaLangClass.common_ptr_);

//#if ENABLE_JAVA_DEBUGGER
//        if (vmDebugReady) {
//            CEModPtr cep = GetCEModifier();
//            cep->loc.classID = GET_CLASS_DEBUGGERID(&CurrentClass->clazz);
//            cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
//            cep->eventKind = JDWP_EventKind_CLASS_LOAD;
//            insertDebugEvent(cep);
//        }
//#endif

//#if INCLUDEDEBUGCODE
//        if (traceclassloading || traceclassloadingverbose) {
//            fprintf(stdout, "Class loaded ok\n");
//        }
//#endif /* INCLUDEDEBUGCODE */
    }
    END_TEMPORARY_ROOTS
}
/*=========================================================================
* FUNCTION:      findSuperMostUnlinked
* TYPE:          constructor (kind of)
* OVERVIEW:      Find the unlinked class highest up on the superclass
*                hierarchy for a given class.
* INTERFACE:
*   parameters:  class structure
*   returns:     super most unlinked class or NULL if there isn't one
*=======================================================================*/
static INSTANCE_CLASS_FAR findSuperMostUnlinked(INSTANCE_CLASS_FAR clazz) {
    INSTANCE_CLASS_FAR result;
    result.common_ptr_ = 0;
    while (clazz.common_ptr_) {
        if (readClassStatus(clazz) < CLASS_LINKED) {
            result.common_ptr_ = clazz.common_ptr_;
        } else {
            break;
        }
        //clazz = clazz->superClass;
        clazz.common_ptr_ = getDWordAt(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS);
    }
    return result;
}

/*=========================================================================
* FUNCTION:      loadConstantPool()
* TYPE:          private class file load operation
* OVERVIEW:      Load the constant pool part of a Java class file,
*                building the runtime class constant pool structure
*                during the loading process.
* INTERFACE:
*   parameters:  classfile pointer
*   returns:     constant pool pointer
*   throws:      ClassFormatError if something goes wrong
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/

static POINTERLIST_FAR loadConstantPool(FILEPOINTER_HANDLE_FAR ClassFileH, INSTANCE_CLASS_FAR CurrentClass) {
    u2 constantCount = loadShort(ClassFileH);
    CONSTANTPOOL_FAR ConstantPool;
    int cpIndex;
    POINTERLIST_FAR result;
    int lastNonUtfIndex = -1;

//#if INCLUDEDEBUGCODE
//    if (traceclassloadingverbose) {
//        fprintf(stdout, "Loading constant pool\n");
//    }
//#endif /* INCLUDEDEBUGCODE */

    /* The classfile constant pool is initially loaded into two temporary
    * structures - one for all the CONSTANT_Utf8 entries (StringPool)
    * and one for all the others (RawPool). Once these constant pool has been
    * parsed into these structures, a permanent constant pool structure
    * (ConstantPool) is created. It's length is determined by the index of
    * the last non-Utf8 entry as we do not save the Utf8 entries in the
    * constant pool after the class has been loaded. All references to Utf8
    * entries are resolved to UString's during loading if they are required
    * after the class has been loaded.
    */
    START_TEMPORARY_ROOTS
        CONSTANTPOOL_ENTRY_FAR pool;
        pool.common_ptr_ = mallocBytes(constantCount * (1 + sizeof(union constantPoolEntryStruct)));
        {
            DECLARE_TEMPORARY_ROOT(CONSTANTPOOL_ENTRY_FAR, RawPool, pool);
            POINTERLIST_FAR plf;
            plf.common_ptr_ = callocObject(SIZEOF_POINTERLIST(constantCount), GCT_POINTERLIST);
            {
                DECLARE_TEMPORARY_ROOT(POINTERLIST_FAR, StringPool, plf);

                //StringPool->length = constantCount;
                setWordAt(StringPool.common_ptr_ + POINTERLIST_LENGTH, constantCount);
//#define RAW_POOL(i)      (RawPool[i])
//#define CP_ENTRY(i)      (ConstantPool->entries[i])

    /* Read the constant pool entries from the class file */
    for (cpIndex = 1; cpIndex < constantCount; cpIndex++) {
        unsigned char tag = loadByte(ClassFileH);
        //unsigned char *Tags = (unsigned char *)(&RawPool[constantCount]);
        PSTR_FAR Tags;
        Tags.common_ptr_ = RawPool.common_ptr_ + constantCount * sizeof(union constantPoolEntryStruct);
        //Tags[cpIndex] = tag;
        setCharAt(Tags.common_ptr_ + cpIndex, tag);

        switch (tag) {
            case CONSTANT_String:
            case CONSTANT_Class: {
                /* A single 16-bit entry that points to a UTF string */
                u2 nameIndex = loadShort(ClassFileH);
                //RAW_POOL(cpIndex).integer = nameIndex;
                setWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), nameIndex);
                break;
            }

            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref: {
                /* Two 16-bit entries */
                u2 classIndex = loadShort(ClassFileH);
                u2 nameTypeIndex = loadShort(ClassFileH);
                //RAW_POOL(cpIndex).method.classIndex = classIndex;
                setDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), classIndex);
                //RAW_POOL(cpIndex).method.nameTypeIndex = nameTypeIndex;
                setDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct) + 2, nameTypeIndex);
                break;
                                              }

            case CONSTANT_Float:
//#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
//#endif
            case CONSTANT_Integer:
                {
                    /* A single 32-bit value */
                    u4 value = loadCell(ClassFileH);
                    //RAW_POOL(cpIndex).integer = value;
                    setDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), value);

                    break;
                }

            case CONSTANT_Double:
//#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
//#endif
            case CONSTANT_Long:
                /* A 64-bit value */

                //RAW_POOL(cpIndex).integer = loadCell(ClassFileH);
                setDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), loadCell(ClassFileH));
                cpIndex++;
                if (cpIndex >= constantCount) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_BAD_64BIT_CONSTANT);
                    raiseExceptionWithMessage(ClassFormatError, msg);
                }
                /* set this location in the Tags array to 0 so it's
                * flagged as a bogus location if some TCK test decides
                * to read from the middle of this long constant pool entry.
                */
                //Tags[cpIndex] = 0;
                setCharAt(Tags.common_ptr_ + cpIndex, 0);
                //RAW_POOL(cpIndex).integer = loadCell(ClassFileH);
                setDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), loadCell(ClassFileH));
                break;

            case CONSTANT_NameAndType: {
                /* Like Fieldref, etc */
                u2 nameIndex = loadShort(ClassFileH);
                u2 typeIndex = loadShort(ClassFileH);
                /* In the second pass, below, these will be replaced with the
                * actual nameKey and typeKey.  Currently, they are indices
                * to items in the constant pool that may not yet have been
                * loaded */
                //RAW_POOL(cpIndex).nameTypeKey.nt.nameKey = nameIndex;
                setWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), nameIndex);
                //RAW_POOL(cpIndex).nameTypeKey.nt.typeKey = typeIndex;
                setWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct) + 2, typeIndex);
                break;
                                       }

            case CONSTANT_Utf8: {
                    u2 length = loadShort(ClassFileH);
                    /* This allocation may invalidate ClassFile */
                    //char *string = mallocBytes(length + 1);
                    far_ptr string = mallocBytes(length + 1);
                    //STRING_POOL(cpIndex) =  string;
                    setDWordAt(StringPool.common_ptr_ + POINTERLIST_DATA + cpIndex * sizeof(cellOrPointer), string);
                    loadBytes(ClassFileH, string, length);
                    //string[length] = '\0';
                    setCharAt(string + length, 0);
                    verifyUTF8String(string, length);
                }
                break;

            default:
                {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_INVALID_CONSTANT_POOL_ENTRY);
                    raiseExceptionWithMessage(ClassFormatError, msg);

                }
                break;
        }

        if (tag != CONSTANT_Utf8) {
            lastNonUtfIndex = cpIndex;
        }
    }

    {
        /* Allocate memory for all the entries plus the array of tag bytes
        * at the end.
        */
        u2 numberOfEntries = lastNonUtfIndex + 1;
        u2 tableSize = numberOfEntries/* + ((numberOfEntries + (CELL - 1)) >> log2CELL)*/;
//#if USESTATIC
//        IS_TEMPORARY_ROOT(ConstantPool,
//            (CONSTANTPOOL)callocObject(tableSize, GCT_NOPOINTERS));
//#else
        ConstantPool.common_ptr_ = callocPermanentObject(tableSize);
//#endif
        //CONSTANTPOOL_LENGTH(ConstantPool) = numberOfEntries;
        setWordAt(ConstantPool.common_ptr_, numberOfEntries);
        //CurrentClass->constPool = ConstantPool;
        setDWordAt(CurrentClass.common_ptr_ + INSTANCE_CLASS_CONSTPOOL, ConstantPool.common_ptr_);
    }

    /* Now create the constant pool entries */
    for (cpIndex = 1; cpIndex < constantCount; cpIndex++) {
        /* These can move between iterations of the loop */
        //unsigned char *Tags = (unsigned char *)(&RawPool[constantCount]);
        PSTR_FAR Tags;
        PSTR_FAR CPTags;
        u1 tag;
        Tags.common_ptr_ = RawPool.common_ptr_ + constantCount * sizeof(union constantPoolEntryStruct);

        //unsigned char *CPTags = CONSTANTPOOL_TAGS(ConstantPool);
        CPTags.common_ptr_ = ConstantPool.common_ptr_ + sizeof(union constantPoolEntryStruct) * getDWordAt(ConstantPool.common_ptr_);

        //unsigned char tag = Tags[cpIndex];
        tag = getCharAt(Tags.common_ptr_ + cpIndex);
        if (cpIndex <= lastNonUtfIndex) {
            //CPTags[cpIndex] = tag;
            setCharAt(CPTags.common_ptr_ + cpIndex, tag);
        }

        switch (tag) {

            case CONSTANT_Class: {
                u2 nameIndex = getWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct));
                POINTERLIST_HANDLE_FAR plhf;
                plhf.common_ptr_ = (far_ptr)&StringPool;
                START_TEMPORARY_ROOTS
                    DECLARE_TEMPORARY_ROOT(PSTR_FAR, name, getUTF8String(plhf, nameIndex));
                    verifyName(name, LegalClass);
                    //CP_ENTRY(cpIndex).clazz = getRawClassX(&name, 0, strlen(name));
                    {
                        CONST_CHAR_HANDLE_FAR cchf;
                        cchf.common_ptr_ = (far_ptr)&name;
                        setDWordAt(ConstantPool.common_ptr_ + sizeof(union constantPoolEntryStruct) * cpIndex, getRawClassX(cchf, 0, hstrlen(name.common_ptr_)).common_ptr_);
                    }
                END_TEMPORARY_ROOTS
                    break;
                                 }

            case CONSTANT_String: {
                u2 nameIndex = getWordAt(RawPool.common_ptr_ + cpIndex)/* (unsigned short)RAW_POOL(cpIndex).integer*/;
                POINTERLIST_HANDLE_FAR plhf;
                plhf.common_ptr_ = (far_ptr)&StringPool;
                {
                    PSTR_FAR name = getUTF8String(plhf, nameIndex);
                    INTERNED_STRING_INSTANCE_FAR string = internString(name, hstrlen(name.common_ptr_));
                    //CP_ENTRY(cpIndex).String = string;
                    setDWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), string.common_ptr_);
                }
                break;
                                  }
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref: {
                /* Two 16-bit entries */
                //u2 classIndex = RAW_POOL(cpIndex).method.classIndex;
                //u2 nameTypeIndex =RAW_POOL(cpIndex).method.nameTypeIndex;
                const u2 classIndex = getWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct));
                const u2 nameTypeIndex = getWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct) + 2);
                if (classIndex >= constantCount ||
                    getCharAt(Tags.common_ptr_ + classIndex) != CONSTANT_Class ||
                    nameTypeIndex >= constantCount ||
                    getCharAt(Tags.common_ptr_ + nameTypeIndex) != CONSTANT_NameAndType) {
                        PSTR_FAR msg;
                        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_FIELD_OR_METHOD_REFERENCE);
                        raiseExceptionWithMessage(ClassFormatError, msg);
                } else {
                    //unsigned short nameIndex = RAW_POOL(nameTypeIndex).nameTypeKey.nt.nameKey;
                    //unsigned short typeIndex = RAW_POOL(nameTypeIndex).nameTypeKey.nt.typeKey;
                    const u2 nameIndex = getWordAt(RawPool.common_ptr_ + nameTypeIndex * sizeof(union constantPoolEntryStruct));
                    const u2 typeIndex = getWordAt(RawPool.common_ptr_ + nameTypeIndex * sizeof(union constantPoolEntryStruct) + 2);
                    POINTERLIST_HANDLE_FAR plhf;
                    plhf.common_ptr_ = (far_ptr)&StringPool;
                    {
                        PSTR_FAR name = getUTF8String(plhf, nameIndex);
                        PSTR_FAR type = getUTF8String(plhf, typeIndex);
                        if (   (tag == CONSTANT_Fieldref && getCharAt(type.common_ptr_) == '(')
                            || (tag != CONSTANT_Fieldref && getCharAt(type.common_ptr_) != '(')
                            || (tag != CONSTANT_Fieldref && !hstrcmp(name.common_ptr_, address_24_of("<clinit>")))) {
                                PSTR_FAR msg;
                                msg.common_ptr_ = address_24_of(KVM_MSG_BAD_NAME_OR_TYPE_REFERENCE);
                                raiseExceptionWithMessage(ClassFormatError, msg);
                        }
                        
                        //CP_ENTRY(cpIndex) = RAW_POOL(cpIndex);
                        hmemcpy(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct),
                            RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct),
                            sizeof(union constantPoolEntryStruct));
                    }
                }
                break;
                                              }

            case CONSTANT_Double:
//#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
//#endif
            case CONSTANT_Long:
                //CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                setDWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), getDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct)));
                cpIndex++;
                //CPTags[cpIndex] = 0;
                setCharAt(CPTags.common_ptr_ + cpIndex, 0);
                //CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                setDWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), getDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct)));
                break;

            case CONSTANT_Float:
//#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
//#endif
            case CONSTANT_Integer:
                //CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                setDWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), getDWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct)));
                break;

            case CONSTANT_NameAndType: {
                //unsigned short nameIndex = RAW_POOL(nameTypeIndex).nameTypeKey.nt.nameKey;
                //unsigned short typeIndex = RAW_POOL(nameTypeIndex).nameTypeKey.nt.typeKey;
                const u2 nameIndex = getWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct));
                const u2 typeIndex = getWordAt(RawPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct) + 2);
                POINTERLIST_HANDLE_FAR plhf;
                plhf.common_ptr_ = (far_ptr)&StringPool;
                START_TEMPORARY_ROOTS
                    DECLARE_TEMPORARY_ROOT(PSTR_FAR, name, getUTF8String(plhf, nameIndex));
                    DECLARE_TEMPORARY_ROOT(PSTR_FAR, type, getUTF8String(plhf, typeIndex));
                    NameKey nameKey;
                    u2 typeKey;
                    if (getCharAt(type.common_ptr_) == '(') {
                        verifyName(name, LegalMethod);
                        verifyMethodType(name, type);
                        {
                            CONST_CHAR_HANDLE_FAR cchf;
                            cchf.common_ptr_ = (far_ptr)&type;
                            typeKey = change_MethodSignature_to_Key(cchf, 0, hstrlen(type.common_ptr_));
                        }
                    } else {
                        verifyName(name, LegalField);
                        verifyFieldType(type);
                        {
                            CONST_CHAR_HANDLE_FAR cchf;
                            cchf.common_ptr_ = (far_ptr)&type;
                            typeKey = change_FieldSignature_to_Key(cchf, 0, hstrlen(type.common_ptr_));
                        }
                    }
                    {
                        CONST_CHAR_HANDLE_FAR cchf;
                        cchf.common_ptr_ = (far_ptr)&name;
                        nameKey = change_Name_to_Key(cchf, 0, hstrlen(name.common_ptr_));
               }
                    //CP_ENTRY(cpIndex).nameTypeKey.nt.nameKey = nameKey;
                    //CP_ENTRY(cpIndex).nameTypeKey.nt.typeKey = typeKey;
                    setWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), nameKey);
                    setWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct) + 2, typeKey);
                END_TEMPORARY_ROOTS
                    break;
                                       }

            case CONSTANT_Utf8:
                /* We don't need these after loading time.  So why bother */
                if (cpIndex <= lastNonUtfIndex) {
//                    CP_ENTRY(cpIndex).integer = 0;
                    setWordAt(ConstantPool.common_ptr_ + cpIndex * sizeof(union constantPoolEntryStruct), 0);

                    //CPTags[cpIndex] = 0;
                    setCharAt(CPTags.common_ptr_ + cpIndex, 0);
                }
                break;

            default:{
                PSTR_FAR msg;
                msg.common_ptr_ = address_24_of(KVM_MSG_INVALID_CONSTANT_POOL_ENTRY);
                raiseExceptionWithMessage(ClassFormatError,msg);
                }
                break;
        }
    }
    result = StringPool;
            }
        }
    END_TEMPORARY_ROOTS
        return result;
}

/*=========================================================================
* FUNCTION:      verifyUTF8String()
* TYPE:          private class file load operation
* OVERVIEW:      validate a UTF8 string
* INTERFACE:
*   parameters:  pointer to UTF8 string, length
*   returns:     nothing
*   throws:      ClassFormatError if the string is invalid
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static void verifyUTF8String(far_ptr bytes, unsigned short length)
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

/*=========================================================================
* FUNCTION:      getUTF8String()
* TYPE:          private class file load operation
* OVERVIEW:      get a UTF8 string from string pool, check index validity
* INTERFACE:
*   parameters:  String pool, index
*   returns:     Pointer to UTF8 string
*   throws:      ClassFormatError if the index is invalid
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static PSTR_FAR getUTF8String(POINTERLIST_HANDLE_FAR StringPoolH, u2 index)
{
    POINTERLIST_FAR StringPool;
    PSTR_FAR r;
    StringPool.common_ptr_ = getDWordAt(StringPoolH.common_ptr_);
    if (index >= getWordAt(StringPool.common_ptr_) ||
        getDWordAt(StringPool.common_ptr_ + POINTERLIST_DATA + index * sizeof(cellOrPointer)) == 0) {
            PSTR_FAR msg;
            msg.common_ptr_ = address_24_of(KVM_MSG_BAD_UTF8_INDEX);
            raiseExceptionWithMessage(ClassFormatError, msg);
    }
    r.common_ptr_ = getDWordAt(StringPool.common_ptr_ + POINTERLIST_DATA + index * sizeof(cellOrPointer));
    return r;
}


/*=========================================================================
* FUNCTION:      verifyName()
* TYPE:          private class file load operation
* OVERVIEW:      validate a class, field, or method name
* INTERFACE:
*   parameters:  pointer to a name, name type
*   returns:     a boolean indicating the validity of the name
*   throws:      ClassFormatError if the name is invalid
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static void verifyName(PSTR_FAR name, enum validName_type type)
{
    if (!isValidName(name,type)) {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_NAME);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }
}

/*=========================================================================
* FUNCTION:      verifyMethodType()
* TYPE:          private class file load operation
* OVERVIEW:      validate method signature
* INTERFACE:
*   parameters:  method name, signature
*   returns:     argument size
*   throws:      ClassFormatError if the signature is invalid
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static u2 verifyMethodType(PSTR_FAR name, PSTR_FAR signature)
{
    u2 args_size = 0;
    PSTR_FAR p = signature;
    u2 length = hstrlen(signature.common_ptr_);
    PSTR_FAR next_p;

    /* The first character must be a '(' */
    if ((length > 0) && (getCharAt(p.common_ptr_++) == '(')) {
        length--;
        /* Skip over however many legal field signatures there are */
        while ((length > 0) &&
            (next_p = skipOverFieldType(p, FALSE, length)).common_ptr_ != 0) {
                args_size++;
                if (getCharAt(p.common_ptr_) == 'J' || getCharAt(p.common_ptr_) == 'D')
                    args_size++;
                length -= (next_p.common_ptr_ - p.common_ptr_);
                p = next_p;
        }
        /* The first non-signature thing better be a ')' */
        if ((length > 0) && (getCharAt(p.common_ptr_++) == ')')) {
            length --;
            if (hstrlen(name.common_ptr_) > 0 && getCharAt(name.common_ptr_) == '<') {
                /* All internal methods must return void */
                if ((length == 1) && (getCharAt(p.common_ptr_) == 'V'))
                    return args_size;
            } else {
                /* Now, we better just have a return value. */
                next_p =  skipOverFieldType(p, TRUE, length);
                if (next_p.common_ptr_ != 0 && (length == next_p.common_ptr_ - p.common_ptr_))
                    return args_size;
            }
        }
    }
    {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_METHOD_SIGNATURE);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }
    return 0; /* never reached */
}


/*=========================================================================
* FUNCTION:      verifyFieldType()
* TYPE:          private class file load operation
* OVERVIEW:      validate field signature
* INTERFACE:
*   parameters:  field signature
*   returns:     nothing
*   throws:      ClassFormatError if the signature is invalid
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static void verifyFieldType(PSTR_FAR type)
{
    u2 length = hstrlen(type.common_ptr_);
    PSTR_FAR p = skipOverFieldType(type, FALSE, length);

    if (p.common_ptr_ == 0 || p.common_ptr_ - type.common_ptr_ != length) {
        PSTR_FAR msg;
        msg.common_ptr_ = address_24_of(KVM_MSG_BAD_FIELD_SIGNATURE);
        raiseExceptionWithMessage(ClassFormatError, msg);
    }
}



/*=========================================================================
* FUNCTION:      skipOverFieldType()
* TYPE:          private class file load operation
* OVERVIEW:      skip over a legal field signature in a get a given string
* INTERFACE:
*   parameters:  string: a string in which the field signature is skipped
*                slash_okay: is '/' OK.
*                length: length of the string
*   returns:     what's remaining after skipping the field signature
*=======================================================================*/
static PSTR_FAR skipOverFieldType(PSTR_FAR string, BOOL void_okay, u2 length)
{
    PSTR_FAR r;
    unsigned int depth = 0;
    for (;length > 0;) {
        switch (getCharAt(string.common_ptr_)) {
        case 'V':
            if (!void_okay) {
                r.common_ptr_ = 0;
                return r;
            }
            /* FALL THROUGH */
        case 'Z':
        case 'B':
        case 'C':
        case 'S':
        case 'I':
        case 'J':
//#if IMPLEMENTS_FLOAT
//        case 'F':
//        case 'D':
//#endif
            string.common_ptr_++;
            return string;

        case 'L': {
            /* Skip over the class name, if one is there. */
            PSTR_FAR s1;
            s1.common_ptr_ = string.common_ptr_ + 1;
            {
                PSTR_FAR p = skipOverFieldName(s1, TRUE, (unsigned short) (length - 1));
                /* The next character better be a semicolon. */
                if (p.common_ptr_ != 0 && p.common_ptr_ < string.common_ptr_ + length && getCharAt(p.common_ptr_) == ';') {
                    p.common_ptr_++;
                    return p;
                }
                r.common_ptr_ = 0;
                return r;

            }
                  }

        case '[':
            /* The rest of what's there better be a legal signature.  */
            string.common_ptr_++;
            length--;
            if (++depth == 256) {
                r.common_ptr_ = 0;
                return r;
            }
            void_okay = FALSE;
            break;

        default:
            r.common_ptr_ = 0;
            return r;
        }
    }
    r.common_ptr_ = 0;
    return r;
}


/*=========================================================================
* FUNCTION:      skipOverFieldName()
* TYPE:          private class file load operation
* OVERVIEW:      skip over a legal field name in a get a given string
* INTERFACE:
*   parameters:  string: a string in which the field name is skipped
*                slash_okay: is '/' OK.
*                length: length of the string
*   returns:     what's remaining after skipping the field name
*=======================================================================*/
static PSTR_FAR skipOverFieldName(PSTR_FAR string, BOOL slash_okay, u2 length)
{
    PSTR_FAR p;
    unsigned short ch;
    unsigned short last_ch = 0;
    PSTR_FAR zero;
    zero.common_ptr_ = 0;
    /* last_ch == 0 implies we are looking at the first char. */
    for (p = string; p.common_ptr_ != string.common_ptr_ + length; last_ch = ch) {
        PSTR_FAR old_p = p;
        ch = getCharAt(p.common_ptr_);
        if (ch < 128) {
            p.common_ptr_++;
            /* quick check for ascii */
            if ((ch >= 'a' && ch <= 'z') ||
                (ch >= 'A' && ch <= 'Z') ||
                (last_ch && ch >= '0' && ch <= '9')) {
                    continue;
            }
        } else {
            /* KVM simplification: we give up checking for those Unicode
            chars larger than 127. Otherwise we'll have to include a
            couple of large Unicode tables, bloating the footprint by
            8~10K. */
            PSTR_FAR tmp_p = p;
            ch = utf2unicode(&tmp_p);
            p = tmp_p;
            continue;
        }

        if (slash_okay && ch == '/' && last_ch) {
            if (last_ch == '/') {
                /* Don't permit consecutive slashes */
                return zero;           
            }
        } else if (ch == '_' || ch == '$') {
            /* continue */
        } else {
            return last_ch ? old_p : zero;
        }
    }
    return last_ch ? p : zero;
}


/*=========================================================================
* FUNCTION:      loadVersionInfo()
* TYPE:          private class file load operation
* OVERVIEW:      Load the first few bytes of a Java class file,
*                checking the file type and version information.
* INTERFACE:
*   parameters:  classfile pointer
*   returns:     <nothing>
*   throws:      ClassFormatError if this is not a Java class file
*                (this error class is not supported by CLDC 1.0 or 1.1)
*=======================================================================*/
static void loadVersionInfo(FILEPOINTER_HANDLE_FAR ClassFileH)
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
