/* project : javaonzx
   file    : locader.c
   purpose : class loader
   author  : valker
*/

#include <stddef.h>
#include <stdio.h>
#include <string.h>
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

#if INCLUDEDEBUGCODE
            if (traceclassloading || traceclassloadingverbose) {
                fprintf(stdout, "Linking class: '%s'\n",
                    getClassName((CLASS)clazz));
            }
#endif

            /*
            * Link the superclass.
            */
            if (getWordAt(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS) == 0) {
                /*
                * This must be java.lang.Object.
                */
                if (clazz.common_ptr_ != JavaLangObject.common_ptr_) {
                    PSTR_FAR msg;
                    msg.common_ptr_ = address_24_of(KVM_MSG_BAD_SUPERCLASS);
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

#if ENABLE_JAVA_DEBUGGER
            if (vmDebugReady) {
                CEModPtr cep = GetCEModifier();
                cep->loc.classID = GET_CLASS_DEBUGGERID(&clazz->clazz);
                cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
                cep->eventKind = JDWP_EventKind_CLASS_PREPARE;
                insertDebugEvent(cep);
            }
#endif

#if INCLUDEDEBUGCODE
            if (traceclassloading || traceclassloadingverbose) {
                fprintf(stdout, "Class linked ok\n");
            }
#endif /* INCLUDEDEBUGCODE */
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

#if INCLUDEDEBUGCODE
    if (traceclassloading || traceclassloadingverbose) {
        Log->loadClass(getClassName((CLASS)CurrentClass));
    }
#endif
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

#if ROMIZING
        UString uPackageName = CurrentClass->clazz.packageName;
        if (uPackageName != NULL) {
            char *name = UStringInfo(uPackageName);
            if (IS_RESTRICTED_PACKAGE_NAME(name)) {
                raiseExceptionWithMessage(NoClassDefFoundError,
                    KVM_MSG_CREATING_CLASS_IN_SYSTEM_PACKAGE);
            }
        }
#endif

        /* This flag is true only when class loading is performed */
        /* from Class.forName() native method. Only the initiating */
        /* class (the one being loaded via Class.forName() should */
        /* cause a different exception to be thrown */
        loadedReflectively = FALSE;


        {
            FILEPOINTER_HANDLE_FAR fphf;
            fphf.common_ptr_ = &ClassFile;
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
                    plhf.common_ptr_ = &StringPool;
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

#if ENABLE_JAVA_DEBUGGER
        if (vmDebugReady) {
            CEModPtr cep = GetCEModifier();
            cep->loc.classID = GET_CLASS_DEBUGGERID(&CurrentClass->clazz);
            cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
            cep->eventKind = JDWP_EventKind_CLASS_LOAD;
            insertDebugEvent(cep);
        }
#endif

#if INCLUDEDEBUGCODE
        if (traceclassloading || traceclassloadingverbose) {
            fprintf(stdout, "Class loaded ok\n");
        }
#endif /* INCLUDEDEBUGCODE */
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

#if INCLUDEDEBUGCODE
    if (traceclassloadingverbose) {
        fprintf(stdout, "Loading constant pool\n");
    }
#endif /* INCLUDEDEBUGCODE */

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
#define RAW_POOL(i)      (RawPool[i])
#define CP_ENTRY(i)      (ConstantPool->entries[i])

    /* Read the constant pool entries from the class file */
    for (cpIndex = 1; cpIndex < constantCount; cpIndex++) {
        unsigned char tag = loadByte(ClassFileH);
        unsigned char *Tags = (unsigned char *)(&RawPool[constantCount]);
        Tags[cpIndex] = tag;

        switch (tag) {
            case CONSTANT_String:
            case CONSTANT_Class: {
                /* A single 16-bit entry that points to a UTF string */
                unsigned short nameIndex = loadShort(ClassFileH);
                RAW_POOL(cpIndex).integer = nameIndex;
                break;
                                 }

            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref: {
                /* Two 16-bit entries */
                unsigned short classIndex = loadShort(ClassFileH);
                unsigned short nameTypeIndex = loadShort(ClassFileH);
                RAW_POOL(cpIndex).method.classIndex = classIndex;
                RAW_POOL(cpIndex).method.nameTypeIndex = nameTypeIndex;
                break;
                                              }

            case CONSTANT_Float:
#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
#endif
            case CONSTANT_Integer:
                {
                    /* A single 32-bit value */
                    long value = loadCell(ClassFileH);
                    RAW_POOL(cpIndex).integer = value;
                    break;
                }

            case CONSTANT_Double:
#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
#endif
            case CONSTANT_Long:
                /* A 64-bit value */
                RAW_POOL(cpIndex).integer = loadCell(ClassFileH);
                cpIndex++;
                if (cpIndex >= constantCount) {
                    raiseExceptionWithMessage(ClassFormatError,
                        KVM_MSG_BAD_64BIT_CONSTANT);
                }
                /* set this location in the Tags array to 0 so it's
                * flagged as a bogus location if some TCK test decides
                * to read from the middle of this long constant pool entry.
                */
                Tags[cpIndex] = 0;
                RAW_POOL(cpIndex).integer = loadCell(ClassFileH);
                break;

            case CONSTANT_NameAndType: {
                /* Like Fieldref, etc */
                unsigned short nameIndex = loadShort(ClassFileH);
                unsigned short typeIndex = loadShort(ClassFileH);
                /* In the second pass, below, these will be replaced with the
                * actual nameKey and typeKey.  Currently, they are indices
                * to items in the constant pool that may not yet have been
                * loaded */
                RAW_POOL(cpIndex).nameTypeKey.nt.nameKey = nameIndex;
                RAW_POOL(cpIndex).nameTypeKey.nt.typeKey = typeIndex;
                break;
                                       }

            case CONSTANT_Utf8: {
                unsigned short length = loadShort(ClassFileH);
                /* This allocation may invalidate ClassFile */
                char *string = mallocBytes(length + 1);
                STRING_POOL(cpIndex) =  string;
                loadBytes(ClassFileH, string, length);
                string[length] = '\0';

                verifyUTF8String(string, length);
                                }
                                break;

            default:
                raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_INVALID_CONSTANT_POOL_ENTRY);
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
        int numberOfEntries = lastNonUtfIndex + 1;
        int tableSize =
            numberOfEntries + ((numberOfEntries + (CELL - 1)) >> log2CELL);
#if USESTATIC
        IS_TEMPORARY_ROOT(ConstantPool,
            (CONSTANTPOOL)callocObject(tableSize, GCT_NOPOINTERS));
#else
        ConstantPool = (CONSTANTPOOL)callocPermanentObject(tableSize);
#endif
        CONSTANTPOOL_LENGTH(ConstantPool) = numberOfEntries;
        CurrentClass->constPool = ConstantPool;
    }

    /* Now create the constant pool entries */
    for (cpIndex = 1; cpIndex < constantCount; cpIndex++) {
        /* These can move between iterations of the loop */
        unsigned char *Tags = (unsigned char *)(&RawPool[constantCount]);
        unsigned char *CPTags = CONSTANTPOOL_TAGS(ConstantPool);

        unsigned char tag = Tags[cpIndex];
        if (cpIndex <= lastNonUtfIndex) {
            CPTags[cpIndex] = tag;
        }

        switch (tag) {

            case CONSTANT_Class: {
                unsigned short nameIndex =
                    (unsigned short)RAW_POOL(cpIndex).integer;
                START_TEMPORARY_ROOTS
                    DECLARE_TEMPORARY_ROOT(const char *, name,
                getUTF8String(&StringPool, nameIndex));
                verifyName(name, LegalClass);
                CP_ENTRY(cpIndex).clazz =
                    getRawClassX(&name, 0, strlen(name));
                END_TEMPORARY_ROOTS
                    break;
                                 }

            case CONSTANT_String: {
                unsigned short nameIndex =
                    (unsigned short)RAW_POOL(cpIndex).integer;
                char *name = getUTF8String(&StringPool, nameIndex);
                INTERNED_STRING_INSTANCE string =
                    internString(name, strlen(name));
                CP_ENTRY(cpIndex).String = string;
                break;
                                  }
            case CONSTANT_Fieldref:
            case CONSTANT_Methodref:
            case CONSTANT_InterfaceMethodref: {
                /* Two 16-bit entries */
                unsigned short classIndex = RAW_POOL(cpIndex).method.classIndex;
                unsigned short nameTypeIndex =RAW_POOL(cpIndex).method.nameTypeIndex;
                if (classIndex >= constantCount ||
                    Tags[classIndex] != CONSTANT_Class ||
                    nameTypeIndex >= constantCount ||
                    Tags[nameTypeIndex] != CONSTANT_NameAndType) {
                        raiseExceptionWithMessage(ClassFormatError,
                            KVM_MSG_BAD_FIELD_OR_METHOD_REFERENCE);
                } else {
                    unsigned short nameIndex =
                        RAW_POOL(nameTypeIndex).nameTypeKey.nt.nameKey;
                    unsigned short typeIndex =
                        RAW_POOL(nameTypeIndex).nameTypeKey.nt.typeKey;
                    char* name = getUTF8String(&StringPool, nameIndex);
                    char* type = getUTF8String(&StringPool, typeIndex);
                    if (   (tag == CONSTANT_Fieldref && type[0] == '(')
                        || (tag != CONSTANT_Fieldref && type[0] != '(')
                        || (tag != CONSTANT_Fieldref &&
                        !strcmp(name, "<clinit>"))) {
                            raiseExceptionWithMessage(ClassFormatError,
                                KVM_MSG_BAD_NAME_OR_TYPE_REFERENCE);
                    }
                    CP_ENTRY(cpIndex) = RAW_POOL(cpIndex);
                }
                break;
                                              }

            case CONSTANT_Double:
#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
#endif
            case CONSTANT_Long:
                CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                cpIndex++;
                CPTags[cpIndex] = 0;
                CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                break;

            case CONSTANT_Float:
#if !IMPLEMENTS_FLOAT
                fatalError(KVM_MSG_FLOATING_POINT_NOT_SUPPORTED);
#endif
            case CONSTANT_Integer:
                CP_ENTRY(cpIndex).integer = RAW_POOL(cpIndex).integer;
                break;

            case CONSTANT_NameAndType: {
                unsigned short nameIndex = RAW_POOL(cpIndex).nameTypeKey.nt.nameKey;
                unsigned short typeIndex = RAW_POOL(cpIndex).nameTypeKey.nt.typeKey;
                START_TEMPORARY_ROOTS
                    DECLARE_TEMPORARY_ROOT(const char *, name,
                getUTF8String(&StringPool, nameIndex));
                DECLARE_TEMPORARY_ROOT(const char *, type,
                getUTF8String(&StringPool, typeIndex));
                NameKey nameKey;
                unsigned short typeKey;
                if (type[0] == '(') {
                    verifyName(name, LegalMethod);
                    verifyMethodType(name, type);
                    typeKey = change_MethodSignature_to_Key(&type, 0,
                        strlen(type));
                } else {
                    verifyName(name, LegalField);
                    verifyFieldType(type);
                    typeKey = change_FieldSignature_to_Key(&type, 0,
                        strlen(type));
                }
                nameKey = change_Name_to_Key(&name, 0, strlen(name));
                CP_ENTRY(cpIndex).nameTypeKey.nt.nameKey = nameKey;
                CP_ENTRY(cpIndex).nameTypeKey.nt.typeKey = typeKey;
                END_TEMPORARY_ROOTS
                    break;
                                       }

            case CONSTANT_Utf8:
                /* We don't need these after loading time.  So why bother */
                if (cpIndex <= lastNonUtfIndex) {
                    CP_ENTRY(cpIndex).integer = 0;
                    CPTags[cpIndex] = 0;
                }
                break;

            default:
                raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_INVALID_CONSTANT_POOL_ENTRY);
                break;
        }
    }
    result = StringPool;
            }
        }
    END_TEMPORARY_ROOTS
        return result;
}
