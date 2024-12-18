/* project : javaonzx
   file    : class.c
   purpose : Internal runtime class structures
   author  : valker
*/

#include <string.h>
#include <stdio.h>
#include <stddef.h>
#ifdef ZX
#include <intrz80.h>
#endif
#include "jvm_types.h"
#include "garbage.h"
#include "class.h"
#include "hashtable.h"
#include "pool.h"
#include "seh.h"
#include "loader.h"
#include "root_code.h"
#include "messages.h"
#include "fields.h"
#include "interpret.h"
#include "frame.h"
#include "verifier.h"
#include "thread.h"

THROWABLE_INSTANCE_FAR OutOfMemoryObject;
THROWABLE_INSTANCE_FAR StackOverflowObject;

INSTANCE_CLASS_FAR JavaLangObject;    /* Pointer to class 'java.lang.Object' */
INSTANCE_CLASS_FAR JavaLangClass;     /* Pointer to class 'java.lang.Class' */
INSTANCE_CLASS_FAR JavaLangString;    /* Pointer to class 'java.lang.String' */

INSTANCE_CLASS_FAR JavaLangSystem;
INSTANCE_CLASS_FAR JavaLangThread;
INSTANCE_CLASS_FAR JavaLangThrowable;
INSTANCE_CLASS_FAR JavaLangError;

INSTANCE_CLASS_FAR JavaLangOutOfMemoryError;



ARRAY_CLASS_FAR PrimitiveArrayClasses[T_LASTPRIMITIVETYPE + 1];

NameTypeKey initNameAndType;   /* void <init>() */
NameTypeKey clinitNameAndType; /* void <clinit>() */
NameTypeKey runNameAndType;    /* void run() */
NameTypeKey mainNameAndType;   /* void main(String[]) */

METHOD_FAR RunCustomCodeMethod;

static void runClinit(FRAME_HANDLE_FAR);
static void runClinitException(FRAME_HANDLE_FAR);


CLASS_FAR getRawClassX(CONST_CHAR_HANDLE_FAR nameH, i2 offset, i2 length) {
    CLASS_FAR result;
    //    const char *start = unhand(nameH);
    PSTR_FAR start;
    //const char *className = start + offset;
    PSTR_FAR className;
    //const char *firstNonBracket, *p;
    PSTR_FAR firstNonBracket;
    PSTR_FAR p;
    int depth;

    readHmem(&start, nameH.common_ptr_, sizeof(start));
    className.common_ptr_ = start.common_ptr_ + offset;

    /* Find the firstNonBracket character, and the last slash in the string */
    for (p = className; getCharAt(p.common_ptr_) == '['; p.common_ptr_++) {};
    firstNonBracket = p;
    depth = p.common_ptr_ - className.common_ptr_;

    if (depth == 0) {
        UString_FAR packageName, baseName;
        for (p.common_ptr_ = className.common_ptr_ + length ; ;) {
            --p.common_ptr_;
            if (getCharAt(p.common_ptr_) == '/') {
                int packageNameLength = p.common_ptr_ - className.common_ptr_;
                int baseNameLength = (length - 1) - packageNameLength;
                packageName = getUStringX(nameH, offset, packageNameLength);
                /* p and start may no longer be valid pointers, but there
                * difference is still a valid offset */
                baseName = getUStringX(nameH, (p.common_ptr_ + 1) - start.common_ptr_, baseNameLength);
                break;
            } else if (p.common_ptr_ == className.common_ptr_) {
                packageName.common_ptr_ = 0;
                baseName = getUStringX(nameH, offset, length);
                break;
            }
        }
        result = change_Name_to_CLASS(packageName, baseName);
        return result;
    } else if (depth + 1 == length) {
        /* An array of some primitive type */
        INSTANCE_CLASS_FAR zero = {0};
        result.common_ptr_ = getArrayClass(depth, zero , getCharAt(firstNonBracket.common_ptr_)).common_ptr_;
        return result;
    } else {
        INSTANCE_CLASS_FAR baseClass;
        //const char *baseClassStart = firstNonBracket + 1; /* skip the 'L' */
        PSTR_FAR baseClassStart;
        //const char *baseClassEnd = className + length - 1;  /* skip final ';' */
        PSTR_FAR baseClassEnd;
        baseClassStart.common_ptr_ = firstNonBracket.common_ptr_ + 1;
        baseClassEnd.common_ptr_ = className.common_ptr_ + length - 1;

        baseClass.common_ptr_ = getRawClassX(nameH, baseClassStart.common_ptr_ - start.common_ptr_, baseClassEnd.common_ptr_  - baseClassStart.common_ptr_).common_ptr_;
        /* The call to getArrayClass but we don't have any pointers any more. */
        result.common_ptr_ = getArrayClass(depth, baseClass, '\0').common_ptr_;
        return result;
    }
}

CLASS_FAR getClassX(CONST_CHAR_HANDLE_FAR nameH, int offset, int length) {
    CLASS_FAR clazz = getRawClassX(nameH, offset, length);
    if (!IS_ARRAY_CLASS(clazz)) {
        INSTANCE_CLASS_FAR icf;
        u2 status;
        icf.common_ptr_ = clazz.common_ptr_;
        status = readClassStatus(icf);
        if (status == CLASS_RAW) {
            loadClassfile(icf, TRUE);
        } else if (status == CLASS_ERROR) {
            START_TEMPORARY_ROOTS
                //DECLARE_TEMPORARY_ROOT(char*, className, getClassName(clazz));
                DECLARE_TEMPORARY_ROOT(PSTR_FAR, className, getClassName(clazz));
                raiseExceptionWithMessage(NoClassDefFoundError, className);
            END_TEMPORARY_ROOTS
        }
    }
    return clazz;
}


CLASS_FAR getClass(PSTR_FAR name)
{
    CONST_CHAR_HANDLE_FAR cchf;
    cchf.common_ptr_ = mallocHeapObject(sizeof(CONST_CHAR_HANDLE), GCT_NOPOINTERS);
    writeHmem(&name, cchf.common_ptr_, sizeof(name));
    return getClassX(cchf, 0, hstrlen(name.common_ptr_));
}

CLASS_FAR getRawClass(PSTR_FAR name) {
    CONST_CHAR_HANDLE_FAR cchf;
    cchf.common_ptr_ = mallocHeapObject(sizeof(CONST_CHAR_HANDLE), GCT_NOPOINTERS);
    writeHmem(&name, cchf.common_ptr_, sizeof(name));
    return getRawClassX(cchf, 0, hstrlen(name.common_ptr_));
}


/*=========================================================================
* FUNCTION:      InitializeJavaSystemClasses()
* TYPE:          private constructor
* OVERVIEW:      Load the standard Java system classes needed by the VM.
* INTERFACE:
*   parameters:  <none>
*   returns:     <nothing>
*=======================================================================*/
void InitializeJavaSystemClasses(void) {
//#if !ROMIZING
    int i;
    static const char javaLangObject[] = "java/lang/Object";
    static const char javaLangClass[]  = "java/lang/Class";
    static const char javaLangString[] = "java/lang/String";

    PSTR_FAR    pfJavaLangObject,
                pfJavaLangClass,
                pfJavaLangString;
    pfJavaLangObject.common_ptr_  = address_24_of(&javaLangObject);
    pfJavaLangClass.common_ptr_   = address_24_of(&javaLangClass);
    pfJavaLangString.common_ptr_  = address_24_of(&javaLangString);

    JavaLangObject.common_ptr_ = getRawClass(pfJavaLangObject).common_ptr_;
    JavaLangClass.common_ptr_  = getRawClass(pfJavaLangClass).common_ptr_;
    JavaLangString.common_ptr_ = getRawClass(pfJavaLangString).common_ptr_;

    memset(PrimitiveArrayClasses, 0, sizeof(PrimitiveArrayClasses));
    for (i = T_FIRSTPRIMITIVE_TYPE; i <= T_LASTPRIMITIVETYPE; i++) {
        if (/*!IMPLEMENTS_FLOAT && */(i == T_FLOAT || i == T_DOUBLE)) {
            /* If we're not implementing floats, then don't try to
            * create arrays of floats or arrays of doubles */
        } else {
            INSTANCE_CLASS_FAR zero = {0};
            PrimitiveArrayClasses[i] = getArrayClass(1, zero, typeCodeToSignature((char)i));
        }
    }

    TRY {

        /* Now we can go back and create these for real. . . .*/
        loadClassfile(JavaLangObject, TRUE);
        loadClassfile(JavaLangClass, TRUE);
        loadClassfile(JavaLangString, TRUE);

        /* Load or initialize some other system classes */
        {
            static const char javaLangSystem[] = "java/lang/System";
            static const char javaLangThread[] = "java/lang/Thread";
            static const char javaLangThrowable[] = "java/lang/Throwable";
            static const char javaLangError[] = "java/lang/Error";

            PSTR_FAR    pfJavaLangSystem,
                        pfJavaLangThread,
                        pfJavaLangThrowable,
                        pfJavaLangError;
            pfJavaLangSystem.common_ptr_    = address_24_of(&javaLangSystem);
            pfJavaLangThread.common_ptr_    = address_24_of(&javaLangThread);
            pfJavaLangThrowable.common_ptr_ = address_24_of(&javaLangThrowable);
            pfJavaLangError.common_ptr_     = address_24_of(&javaLangError);

            JavaLangSystem.common_ptr_      = getClass(pfJavaLangSystem).common_ptr_;
            JavaLangThread.common_ptr_      = getClass(pfJavaLangThread).common_ptr_;
            JavaLangThrowable.common_ptr_   = getClass(pfJavaLangThrowable).common_ptr_;
            JavaLangError.common_ptr_       = getClass(pfJavaLangError).common_ptr_;
        }

    } CATCH (e) {
        //fprintf(stdout,"%s", getClassName((CLASS)e->ofClass));
        struct throwableInstanceStruct copy;
        readHmem((void*)&copy, e.common_ptr_, sizeof(struct throwableInstanceStruct));
        if (copy.message.common_ptr_ != 0) {
            //fprintf(stdout, ": %s\n", getStringContents(e->message));
        } else {
            //fprintf(stdout, "\n");
        }
        fatalVMError(KVM_MSG_UNABLE_TO_INITIALIZE_SYSTEM_CLASSES);
    } END_CATCH

//#if INCLUDEDEBUGCODE
//        if (   (JavaLangObject->status == CLASS_ERROR)
//            || (JavaLangClass->status == CLASS_ERROR)
//            || (JavaLangString->status == CLASS_ERROR)
//            || (JavaLangThread == NULL)
//            || (JavaLangThrowable == NULL)
//            || (JavaLangError == NULL)) {
//                fatalVMError(KVM_MSG_UNABLE_TO_INITIALIZE_SYSTEM_CLASSES);
//        }
//#endif /* INCLUDEDEBUGCODE */

//#else /* ROMIZING */
//    InitializeROMImage();
//#endif /* !ROMIZING */

    /* Get handy pointers to special methods */
    {
        static const char initNAT[] = "<init>";
        static const char clinitNAT[] = "<clinit>";
        static const char runNAT[] = "run";
        static const char mainNAT[] = "main";
        static const char voidVoidParam[] = "()V";
        static const char stringsVoidParam[] = "([Ljava/lang/String;)V";
        static const char runCustomCode[] = "runCustomCode";

        PSTR_FAR    pfInitNAT, 
                    pfClinitNAT, 
                    pfRunNAT, 
                    pfMainNAT, 
                    pfVoidVoidParam, 
                    pfStringsVoidParam,
                    pfRunCustomCode;

        pfInitNAT.common_ptr_ = address_24_of(&initNAT);
        pfClinitNAT.common_ptr_ = address_24_of(&clinitNAT);
        pfRunNAT.common_ptr_ = address_24_of(&runNAT);
        pfMainNAT.common_ptr_ = address_24_of(&mainNAT);
        pfVoidVoidParam.common_ptr_ = address_24_of(&voidVoidParam);
        pfStringsVoidParam.common_ptr_ = address_24_of(&stringsVoidParam);
        pfRunCustomCode.common_ptr_ = address_24_of(&runCustomCode);

        initNameAndType   = getNameAndTypeKey(pfInitNAT,   pfVoidVoidParam);
        clinitNameAndType = getNameAndTypeKey(pfClinitNAT, pfVoidVoidParam);
        runNameAndType    = getNameAndTypeKey(pfRunNAT,     pfVoidVoidParam);
        mainNameAndType   = getNameAndTypeKey(pfMainNAT, pfStringsVoidParam);

        RunCustomCodeMethod = getSpecialMethod(JavaLangClass, getNameAndTypeKey(pfRunCustomCode, pfVoidVoidParam));

    }

    /* Patch the bytecode, was a "return" */
    if (inAnyHeap(RunCustomCodeMethod->u.java.code)){
        struct methodStruct copy;
        readHmem((void*)&copy, RunCustomCodeMethod.common_ptr_, sizeof(struct methodStruct));
        //copy.u.java.code[0] = CUSTOMCODE;
        setCharAt(copy.u.java.code.common_ptr_, CUSTOMCODE);
        copy.u.java.maxStack = RunCustomCodeMethod_MAX_STACK_SIZE;
        writeHmem((void*)&copy, RunCustomCodeMethod.common_ptr_, sizeof(struct methodStruct));
    }/* else {
        u1 newcode = CUSTOMCODE;
        short newMaxStack = RunCustomCodeMethod_MAX_STACK_SIZE;
        char *start = (char*)((INSTANCE_CLASS_FAR)JavaLangClass)->constPool;
        int offset = (char*)(RunCustomCodeMethod->u.java.code) - start;
        modifyStaticMemory(start, offset, &newcode, sizeof(newcode));

        offset = (char *)(&RunCustomCodeMethod->u.java.maxStack) - start;
        modifyStaticMemory(start, offset, &newMaxStack, sizeof(newMaxStack));
    }*/
    JavaLangOutOfMemoryError.common_ptr_ = getClass(OutOfMemoryError).common_ptr_;
    OutOfMemoryObject.common_ptr_ = instantiate(JavaLangOutOfMemoryError).common_ptr_;
    makeGlobalRoot(OutOfMemoryObject.common_ptr_);
    
    //StackOverflowObject = OutOfMemoryObject; TODO come back
    
    /* If we had full JLS/JVMS error classes:
    StackOverflowObject =
    (THROWABLE_INSTANCE)instantiate(
    (INSTANCE_CLASS)getClass(StackOverflowError));
    */
    
    //makeGlobalRoot((cell **)&StackOverflowObject); // TODO come back
    
}
//
//
/*=========================================================================
* FUNCTION:      getArrayClass()
* TYPE:          private helper function
* OVERVIEW:      Create an array class.
* INTERFACE:
*   arguments:   depth:     depth of the new array
*                baseClass: if the base type is a class this is nonNull and
*                           contains the base type
*                signCode:  if the base type is a primitive type, then
*                           baseClass must be NULL, and this contains the
*                           letter representing the base type.
*   parameters:  result: class pointer.
*=======================================================================*/

ARRAY_CLASS_FAR
getArrayClass(int depth, INSTANCE_CLASS_FAR baseClass, char signCode)
{
    UString_FAR packageName, baseName;
    BOOL isPrimitiveBase = (baseClass.common_ptr_ == 0);
    ARRAY_CLASS_FAR clazz, result;

    if (isPrimitiveBase) {
        const char *temp = str_buffer;
        packageName.common_ptr_ = 0;
        memset(str_buffer, '[', depth);
        str_buffer[depth] = signCode;
        //baseName = getUStringX(&temp, 0, depth + 1);
        {
            CONST_CHAR_HANDLE_FAR cchf;
            cchf.common_ptr_ = mallocHeapObject(sizeof(PSTR_FAR), GCT_NOPOINTERS);
            writeHmem(&str_buffer, cchf.common_ptr_, sizeof(PSTR_FAR));
            baseName = getUStringX(cchf, 0, depth + 1);
        }
    } else {
        memset(str_buffer, '[', depth);
        {
            struct instanceClassStruct copy;
            readHmem((void*)&copy, baseClass.common_ptr_, sizeof(struct instanceClassStruct));
            sprintf(str_buffer + depth, "L%s;", UStringInfo(copy.clazz.baseName));
            baseName = getUStringNear(str_buffer);
            packageName = copy.clazz.packageName;
        }
    }

    result.common_ptr_ = change_Name_to_CLASS(packageName, baseName).common_ptr_;

    {
        struct arrayClassStruct copy;
        for (clazz.common_ptr_ = result.common_ptr_; readHmem((void*)&copy,clazz.common_ptr_,sizeof(struct arrayClassStruct)), copy.clazz.ofClass.common_ptr_ == 0; ) {
            /* LOOP ASSERT:  depth is the array depth of "clazz" */

            /* This clazz is newly created.  We need to fill in info.  We
            * may need to iterate, since this class's element type may also need
            * to be be created */

            writeHmem((void*)&JavaLangClass, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + CLASS_OF_CLASS, sizeof(JavaLangClass));

        //clazz->clazz.ofClass = JavaLangClass;
        if (depth == 1 && isPrimitiveBase) {
//#if ROMIZING
//            fatalError(KVM_MSG_CREATING_PRIMITIVE_ARRAY);
//#else
            char typeCode;
            switch(signCode) {
                case 'C' : typeCode = T_CHAR    ; break;
                case 'B' : typeCode = T_BYTE    ; break;
                case 'Z' : typeCode = T_BOOLEAN ; break;
//#if IMPLEMENTS_FLOAT
//                case 'F' : typeCode = T_FLOAT   ; break;
//                case 'D' : typeCode = T_DOUBLE  ; break;
//#endif /* IMPLEMENTS_FLOAT */
                case 'S' : typeCode = T_SHORT   ; break;
                case 'I' : typeCode = T_INT     ; break;
                case 'J' : typeCode = T_LONG    ; break;
                case 'V' : typeCode = T_VOID    ; break;
                case 'L' : typeCode = T_CLASS   ; break;
                default : fatalVMError(KVM_MSG_BAD_SIGNATURE); typeCode = 0;
            }
            {
                struct arrayClassStruct copy;
                readHmem((void*)&copy, clazz.common_ptr_, sizeof(struct arrayClassStruct));
                /////
                copy.gcType = GCT_ARRAY;
                copy.itemSize = arrayItemSize(typeCode);
                copy.u.primType = typeCode;
                copy.clazz.accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC | ACC_ARRAY_CLASS;
                copy.clazz.key = signCode + (short)(1 << FIELD_KEY_ARRAY_SHIFT);
                /////
                writeHmem((void*)&copy, clazz.common_ptr_, sizeof(struct arrayClassStruct));
            }
            /* All finished */
//#endif /* ROMIZING */
            break;
        } else {
            //clazz->gcType = GCT_OBJECTARRAY;
            {
                //u4 gcType = GCT_OBJECTARRAY;
                //writeHmem((void*)&gcType,clazz.common_ptr_ + offsetof(struct arrayClassStruct, gcType), sizeof(gcType));
                setDWordAt(clazz.common_ptr_ + offsetof(struct arrayClassStruct, gcType), GCT_OBJECTARRAY);
            }
            //clazz->itemSize = arrayItemSize(T_REFERENCE);
            {
                //u4 itemSize = arrayItemSize(T_REFERENCE);
                //writeHmem((void*)&itemSize,clazz.common_ptr_ + offsetof(struct arrayClassStruct, itemSize), sizeof(itemSize));
                setCharAt(clazz.common_ptr_ + ARRAY_CLASS_ITEMSIZE, arrayItemSize(T_REFERENCE));
            }

            if (isPrimitiveBase) {
                //clazz->clazz.accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC | ACC_ARRAY_CLASS;
                u2 accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC | ACC_ARRAY_CLASS;
                writeHmem((void*)&accessFlags, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + offsetof(struct classStruct, accessFlags), sizeof(accessFlags));
            } else {
                struct instanceClassStruct copy;
                readHmem((void*)&copy, baseClass.common_ptr_, sizeof(struct instanceClassStruct));
                if (copy.status >= CLASS_LOADED) {
//                    clazz->clazz.accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_ARRAY_CLASS | (baseClass->clazz.accessFlags & ACC_PUBLIC);
                    unsigned short accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_ARRAY_CLASS | (copy.clazz.accessFlags & ACC_PUBLIC);
                    writeHmem((void*)&accessFlags, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + offsetof(struct classStruct, accessFlags), sizeof(accessFlags));
                } else {
//                    clazz->clazz.accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_ARRAY_CLASS;
                    unsigned short accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_ARRAY_CLASS;
                    writeHmem((void*)&accessFlags, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + offsetof(struct classStruct, accessFlags), sizeof(accessFlags));
//                    clazz->flags = ARRAY_FLAG_BASE_NOT_LOADED;
                    {
                        long flags = ARRAY_FLAG_BASE_NOT_LOADED;
                        writeHmem((void*)&flags, clazz.common_ptr_ + offsetof(struct arrayClassStruct, flags), sizeof(flags));
                    }

                }
            }
            if (depth >= MAX_FIELD_KEY_ARRAY_DEPTH) {
                //clazz->clazz.key |= (MAX_FIELD_KEY_ARRAY_DEPTH << FIELD_KEY_ARRAY_SHIFT);
                {
                    struct arrayClassStruct copy;
                    readHmem((void*)&copy, clazz.common_ptr_, sizeof(struct arrayClassStruct));
                    copy.clazz.key |= (MAX_FIELD_KEY_ARRAY_DEPTH << FIELD_KEY_ARRAY_SHIFT);
                    writeHmem((void*)&copy, clazz.common_ptr_, sizeof(struct arrayClassStruct));
                }
            } else if (isPrimitiveBase) {
                //clazz->clazz.key = (depth << FIELD_KEY_ARRAY_SHIFT) + signCode;
                unsigned short key = (depth << FIELD_KEY_ARRAY_SHIFT) + signCode;
                writeHmem((void*)&key, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + CLASS_KEY, sizeof(key));
            } else {
                //clazz->clazz.key = (depth << FIELD_KEY_ARRAY_SHIFT) + baseClass->clazz.key;
                unsigned short key;
                readHmem((void*)&key, baseClass.common_ptr_ + ARRAY_CLASS_CLAZZ + CLASS_KEY, sizeof(key));
                key += (depth << FIELD_KEY_ARRAY_SHIFT);
                writeHmem((void*)&key, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + CLASS_KEY, sizeof(key));
            }

            if (depth == 1) {
                /* We must be nonPrimitive.  primitive was handled above */
                //clazz->u.elemClass = (CLASS)baseClass;
                CLASS_FAR elemClass;
                elemClass.common_ptr_ = baseClass.common_ptr_;
                writeHmem((void*)&elemClass, clazz.common_ptr_ + offsetof(struct arrayClassStruct, u), sizeof(elemClass));
                /* All finished */
                break;
            } else {
                /* Class of depth > 1. */
                //UString thisBaseName = clazz->clazz.baseName;
                UString_FAR thisBaseName;
                readHmem((void*)&thisBaseName, clazz.common_ptr_ + ARRAY_CLASS_CLAZZ + offsetof(struct classStruct, baseName), sizeof(baseName));
                {
                    PSTR_FAR baseName = UStringInfo(thisBaseName);
                    CONST_CHAR_HANDLE_FAR cchf;
                    UString_FAR subBaseName;
                    unsigned short length;
                    CLASS_FAR elemClass;
                    cchf.common_ptr_ = mallocHeapObject(sizeof(PSTR_FAR), GCT_NOPOINTERS);
                    writeHmem(&baseName, cchf.common_ptr_, sizeof(baseName));
                    readHmem(&length, thisBaseName.common_ptr_ + UTF_LENGTH, sizeof(length));
                    subBaseName = getUStringX(cchf, 1, length - 1);
                    elemClass = change_Name_to_CLASS(packageName, subBaseName);
                    //clazz->u.elemClass = elemClass;
                    writeHmem((void*)&elemClass, clazz.common_ptr_ + offsetof(struct arrayClassStruct, u), sizeof(elemClass));

                    /* Now "recurse" on elemClass, initialized it, */
                    /* also, if necessary */
                    clazz.common_ptr_ = elemClass.common_ptr_;
                    depth--;
                    continue;
                }
            }
        }
    }
  }
//#if ENABLE_JAVA_DEBUGGER
//    if (vmDebugReady && needEvent) {
//        CEModPtr cep = GetCEModifier();
//        cep->loc.classID = GET_CLASS_DEBUGGERID(&result->clazz);
//        cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
//        cep->eventKind = JDWP_EventKind_CLASS_PREPARE;
//        insertDebugEvent(cep);
//    }
//#endif /* ENABLE_JAVA_DEBUGGER */
    return result;
}



/*=========================================================================
* FUNCTION:      instantiate()
* TYPE:          constructor
* OVERVIEW:      Create an instance of the given class.
* INTERFACE:
*   parameters:  class pointer
*   returns:     pointer to object instance
*
* NOTE:          This operation only allocates the space for the
*                instance and initializes its instance variables to zero.
*                Actual constructor for initializing the variables
*                must be invoked separately.
*=======================================================================*/

INSTANCE_FAR instantiate(INSTANCE_CLASS_FAR thisClass)
{
    u2 instSize;
    INSTANCE_FAR newInstance;
    readHmem(&instSize, thisClass.common_ptr_ + offsetof(struct instanceClassStruct, instSize), sizeof(instSize));
    {
        const u2 size = SIZEOF_INSTANCE(instSize);
        newInstance.common_ptr_ = mallocHeapObject(size, GCT_INSTANCE);

        if (newInstance.common_ptr_ != 0) {
            hmemset(newInstance.common_ptr_, 0, size);
            /* Initialize the class pointer (zeroeth field in the instance) */
            //newInstance->ofClass = thisClass;
            //modifyHmem((void*)&thisClass, newInstance.common_ptr_,)
            writeHmem((void*)&thisClass, newInstance.common_ptr_ + offsetof(struct instanceStruct,ofClass), sizeof(thisClass));
        } else {
            THROW(OutOfMemoryObject);
        }
    }
    return newInstance;
}

/*=========================================================================
* FUNCTION:      arrayItemSize()
* TYPE:          private helper operation for array manipulation
* OVERVIEW:      Get the item size of the array on the basis of its type.
* INTERFACE:
*   parameters:  array type index
*   returns:     array item size
*=======================================================================*/

u1 arrayItemSize(i2 arrayType)
{
    u1 itemSize;

    switch (arrayType) {
    case T_BOOLEAN: case T_BYTE:
        itemSize = 1;
        break;
    case T_CHAR: case T_SHORT:
        itemSize = SHORTSIZE;
        break;
    case T_INT: case T_FLOAT:
        itemSize = CELL;
        break;
    case T_DOUBLE: case T_LONG:
        itemSize = CELL*2;
        break;
    default:
        /* Array of reference type (T_REFERENCE or class pointer) */
        itemSize = CELL;
        break;
    }
    return itemSize;
}

BOOL IS_ARRAY_CLASS(CLASS_FAR clazz) {
    unsigned short accessFlags;
    readHmem((void*)&accessFlags, clazz.common_ptr_ + offsetof(struct classStruct, accessFlags), sizeof(accessFlags));
    return (accessFlags & ACC_ARRAY_CLASS) != 0;
}


/*=========================================================================
* FUNCTION:      getClassName, getClassName_inBuffer
* TYPE:          public instance-level operation on runtime classes
* OVERVIEW:      Gets the name of a class
*
* INTERFACE:
*   parameters:  clazz:       Any class
*                str_buffer:  Where to put the result
*
*   returns:     getClassName() returns a pointer to the result
*                getClassName_inBuffer() returns a pointer to the NULL
*                at the end of the result.  The result is in the
*                passed buffer.
*
* DANGER:  getClassName_inBuffer() does not return what you expect.
*=======================================================================*/

PSTR_FAR getClassName(CLASS_FAR clazz) {
    UString_FAR UPackageName;// = clazz->packageName;
    UString_FAR UBaseName;//    = clazz->baseName;
    readHmem(&UPackageName, clazz.common_ptr_ + offsetof(struct classStruct, packageName), sizeof(UPackageName));
    readHmem(&UBaseName, clazz.common_ptr_ + offsetof(struct classStruct, baseName), sizeof(UBaseName));
    {
        int     baseLength;// = UBaseName->length;
        int     packageLength;
        readHmem(&baseLength, UBaseName.common_ptr_ + UTF_LENGTH, sizeof(baseLength));
        if (UPackageName.common_ptr_ == 0) {
            packageLength = 0;
        } else {
            readHmem(&packageLength, UPackageName.common_ptr_ + UTF_LENGTH, sizeof(baseLength));
        }
        {
            //char *result = mallocBytes(baseLength + packageLength + 5);
            PSTR_FAR result;
            result.common_ptr_ = mallocObject(baseLength + packageLength + 5, GCT_NOPOINTERS);
            getClassName_inBuffer(clazz, result);
            return result;
        }
    }
}

PSTR_FAR                   /* returns pointer to '\0' at end of resultBuffer */
getClassName_inBuffer(CLASS_FAR clazz, PSTR_FAR resultBuffer) {
    UString_FAR UPackageName;
    UString_FAR UBaseName;

    //UPackageName = clazz->packageName;
    //UBaseName    = clazz->baseName;
    readHmem(&UPackageName, clazz.common_ptr_ + offsetof(struct classStruct, packageName), sizeof(UPackageName));
    readHmem(&UBaseName, clazz.common_ptr_ + offsetof(struct classStruct, baseName), sizeof(UBaseName));

    {
        PSTR_FAR baseName = UStringInfo(UBaseName);
        PSTR_FAR from = baseName;      /* pointer into baseName */
        PSTR_FAR to = resultBuffer;    /* used for creating the output */
        u2  fromLength;
        BOOL isArrayOfObject;
        readHmem(&fromLength, UBaseName.common_ptr_ + UTF_LENGTH, sizeof(fromLength));

        /* The result should have exactly as many "["s as the baseName has at
        * its beginning.  Then skip over that part in both the input and the
        * output */
        for (from = baseName; getCharAt(from.common_ptr_) == '['; from.common_ptr_++, fromLength--) {
            setCharAt(to.common_ptr_++, '[');
        }
        /* We're an array of objects if we've actually written something already
        * to "from", and more than one character remains
        */
        isArrayOfObject = (from.common_ptr_ != baseName.common_ptr_ && fromLength != 1);

        if (isArrayOfObject) {
            setCharAt(to.common_ptr_++, 'L');
        }
        /* Now print out the package name, followed by the rest of the baseName */
        if (UPackageName.common_ptr_ != 0) {
            u2 packageLength;// = UPackageName->length;
            readHmem(&packageLength, UPackageName.common_ptr_ + UTF_LENGTH, sizeof(packageLength));
            hmemcpy(to.common_ptr_, UStringInfo(UPackageName).common_ptr_, packageLength);
            to.common_ptr_ += packageLength;
            //*to++ = '/';
            setCharAt(to.common_ptr_++, '/');
        }
        if (isArrayOfObject) {
            hmemcpy(to.common_ptr_, from.common_ptr_ + 1, fromLength - 2); /* skip L and ; */
            to.common_ptr_ += (fromLength - 2);
            //*to++ = ';';
            setCharAt(to.common_ptr_++, ';');
        } else {
            hmemcpy(to.common_ptr_, from.common_ptr_, fromLength);
            to.common_ptr_ += fromLength;
        }
        //*to = '\0';
        setCharAt(to.common_ptr_++, '\0');
        return to;
    }
}


/*=========================================================================
* FUNCTION:      typeCodeToSignature, signatureToTypeCode
* TYPE:          private function
* OVERVIEW:      Converts signature characters to the corresponding
*                type code small integer, and back.
*=======================================================================*/
char typeCodeToSignature(char typeCode) {
    switch(typeCode) {
        case T_CHAR :    return 'C';
        case T_BYTE :    return 'B';
        case T_BOOLEAN : return 'Z';
        case T_FLOAT :   return 'F';
        case T_DOUBLE :  return 'D';
        case T_SHORT :   return 'S';
        case T_INT :     return 'I';
        case T_LONG :    return 'J';
        case T_VOID:     return 'V';
        case T_CLASS:    return 'L';
        default :    fatalVMError(KVM_MSG_BAD_SIGNATURE); return 0;
    }
}


/*=========================================================================
* FUNCTION:      instantiateString(), instantiateInternedString()
* TYPE:          constructor
* OVERVIEW:      Create an initialized Java-level string object.
* INTERFACE:
*   parameters:  UTF8 String containing the value of the new string object
*                number of bytes in the UTF8 string
*   returns:     pointer to array object instance
* NOTE:          String arrays are not intended to be manipulated
*                directly; they are manipulated from within instances
*                of 'java.lang.String'.
*=======================================================================*/
STRING_INSTANCE_FAR instantiateString(PSTR_FAR stringArg, u2 utflength) {
    u2 unicodelength;
    STRING_INSTANCE_FAR result;
    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(PSTR_FAR, string, stringArg);
        DECLARE_TEMPORARY_ROOT(SHORTARRAY_FAR, chars, createCharArray(string, utflength, &unicodelength, FALSE));
        result.common_ptr_ = instantiate(JavaLangString).common_ptr_;
        /* We can't do any garbage collection, since result isn't protected */

        //result->offset = 0;
        setWordAt(result.common_ptr_ + STRINST_OFFSET, 0);
        //result->length = unicodelength;
        setWordAt(result.common_ptr_ + STRINST_LENGTH, unicodelength);
        //result->array = chars;
        setDWordAt(result.common_ptr_ + STRINST_ARRAY, chars.common_ptr_);
    END_TEMPORARY_ROOTS
    return result;
}

INTERNED_STRING_INSTANCE_FAR instantiateInternedString(PSTR_FAR stringArg, u2 utflength)
{
    u2 unicodelength;
    INTERNED_STRING_INSTANCE_FAR result;
    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(PSTR_FAR, string, stringArg);
        SHORTARRAY_FAR chars = createCharArray(string, utflength, &unicodelength, TRUE);
        result.common_ptr_ = callocPermanentObject(SIZEOF_INTERNED_STRING_INSTANCE);
        /* We can't do any garbage collection, since result isn't protected */
        //result->ofClass = JavaLangString;
        setDWordAt(result.common_ptr_, JavaLangString.common_ptr_);
        //result->offset = 0;
        setWordAt(result.common_ptr_ + INTSTRINST_OFFSET, 0);
        //result->length = unicodelength;
        setWordAt(result.common_ptr_ + INTSTRINST_LENGTH, unicodelength);
        //result->array = chars;
        setDWordAt(result.common_ptr_ + INTSTRINST_ARRAY, chars.common_ptr_);
    END_TEMPORARY_ROOTS
    return result;
}



/*=========================================================================
* FUNCTION:      createCharArray()
* TYPE:          constructor (internal use only)
* OVERVIEW:      Create a character array. Handle Utf8 input properly.
*=======================================================================*/

SHORTARRAY_FAR createCharArray(PSTR_FAR utf8stringArg, u2 utf8length, u2 *unicodelengthP, BOOL isPermanent) {
    u2 unicodelength = 0;
    int i;
    SHORTARRAY_FAR newArray;
    PSTR_FAR p, end;
    int size, objSize;

    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(PSTR_FAR, utf8string, utf8stringArg);
        for (p = utf8string, end.common_ptr_ = p.common_ptr_ + utf8length;  p.common_ptr_ < end.common_ptr_;  ) {
            utf2unicode(&p);
            unicodelength++;
        }
        size = (unicodelength * sizeof(u2));
        objSize = SIZEOF_SHORT_ARRAY(size);

        /* Allocate room for the character array */
        newArray.common_ptr_ = isPermanent
            ? 0 //callocPermanentObject(objSize)
            : callocObject(objSize, GCT_ARRAY);

        //newArray->ofClass = PrimitiveArrayClasses[T_CHAR];
        setDWordAt(newArray.common_ptr_, PrimitiveArrayClasses[T_CHAR].common_ptr_);
        //newArray->length  = unicodelength;
        setWordAt(newArray.common_ptr_ + SHORTAR_LENGTH, unicodelength);

        /* Initialize the array with string contents */
        for (p.common_ptr_ = utf8string.common_ptr_, i = 0; i < unicodelength; i++) {
            setWordAt(newArray.common_ptr_ + SHORTAR_SDATA + i * sizeof(u2), utf2unicode(&p));
            //newArray->sdata[i] = utf2unicode(&p);
        }
        *unicodelengthP = unicodelength;
    END_TEMPORARY_ROOTS
    return newArray;
}


/*=========================================================================
* FUNCTION:      revertToRawClass()
* TYPE:          public instance-level operation on runtime classes
* OVERVIEW:      Return a class to the state it is in after being returned
*                getRawClassX the first time.
* INTERFACE:
*   parameters:  class pointer
*   returns:     class pointer
*=======================================================================*/
INSTANCE_CLASS_FAR revertToRawClass(INSTANCE_CLASS_FAR clazz) {
    //memset(&(clazz->superClass),0,
    //    sizeof(struct instanceClassStruct) - sizeof(struct classStruct));
    hmemset(clazz.common_ptr_ + INSTANCE_CLASS_SUPERCLASS, 0, sizeof(struct instanceClassStruct) - sizeof(struct classStruct));
    return clazz;
}


/*=========================================================================
* FUNCTION:      getStringContentsSafely()
* TYPE:          internal operation on string objects
* OVERVIEW:      Get the contents of a string object in C/C++
*                string format.
* INTERFACE:
*   parameters:  String object pointer
*   returns:     char* to the string in C/C++ format
*=======================================================================*/
char* getStringContentsSafely(STRING_INSTANCE_FAR string, char *buf, u2 lth) {
    /* Get the internal unicode array containing the string */
    SHORTARRAY_FAR thisArray;
    u2 offset;
    u2 length;
    int        i;

    thisArray.common_ptr_ = getDWordAt(string.common_ptr_ + STRINST_ARRAY);
    offset = getWordAt(string.common_ptr_ + STRINST_OFFSET);
    length = getWordAt(string.common_ptr_ + STRINST_LENGTH);


    if ((length+1) > lth) {
        fatalError(KVM_MSG_STRINGBUFFER_OVERFLOW);
    }

    /* Copy contents of the unicode array to the C/C++ string */
    for (i = 0; i < length; i++) {
        //buf[i] = (char)thisArray->sdata[offset + i];
        buf[i] = getCharAt(thisArray.common_ptr_ + SHORTAR_SDATA + (offset + i) * sizeof(u2));
    }

    /* Terminate the C/C++ string with zero */
    buf[length] = 0;
    return buf;
}


/*=========================================================================
* FUNCTION:      instantiateArray()
* TYPE:          constructor
* OVERVIEW:      Create an array of given type and size.
* INTERFACE:
*   parameters:  array class, array size
*   returns:     pointer to array object instance
* NOTE:          This operation only allocates the space for the
*                array and initializes its slots to zero.
*                Actual constructor for initializing the array
*                must be invoked separately.
*=======================================================================*/
ARRAY_FAR instantiateArray(ARRAY_CLASS_FAR arrayClass, i4 length) {
    ARRAY_FAR newArray;
    if (length < 0) {
        raiseException(NegativeArraySizeException);
        newArray.common_ptr_ = 0;
        return newArray;
    } else if (length > 0x1000000) {
        /* Don't even try; 'dataSize' below might overflow */
        THROW(OutOfMemoryObject);
    } else {
        /* Does this array contain pointers or not? */
        const GCT_ObjectType gctype = getCharAt(arrayClass.common_ptr_ + ARRAY_CLASS_GCTYPE);
        /* How big is each item */
        const u1 slotSize = getCharAt(arrayClass.common_ptr_ + ARRAY_CLASS_ITEMSIZE);
        /* What's the total data size */
        const u4 dataSize = (length * slotSize);
        /* What's the total size */
        u4 arraySize = SIZEOF_ARRAY(dataSize);

        newArray.common_ptr_ = mallocHeapObject(arraySize, gctype);
        if (newArray.common_ptr_ == 0) {
            THROW(OutOfMemoryObject);
        } else {
            hmemset(newArray.common_ptr_, 0, arraySize);
            //newArray->ofClass   = arrayClass;
            setDWordAt(newArray.common_ptr_ + ARRAYSTRUCT_OFCLASS, arrayClass.common_ptr_);
            //newArray->length    = length;
            setDWordAt(newArray.common_ptr_ + ARRAYSTRUCT_LENGTH, length);
        }
        return newArray;
    }
}


/*=========================================================================
* FUNCTION:      initializeClass()
* TYPE:          constructor
* OVERVIEW:      After loading a class, it must be initialized
*                by executing the possible internal static
*                constructor '<clinit>'. This will initialize the
*                necessary static structures.
*
*                This function sets up the necessary Class.runClinit
*                frame and returns to the interpreter.
* INTERFACE:
*   parameters:  class pointer
*   returns:     <nothing>
*=======================================================================*/
void initializeClass(INSTANCE_CLASS_FAR thisClass) {
    if (getWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_STATUS) == CLASS_ERROR) {
        raiseException(NoClassDefFoundError);
    } else if (getWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_STATUS) < CLASS_READY) {
        if (getWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_STATUS) < CLASS_VERIFIED) {
            verifyClass(thisClass);
        }
        /*
        * VerifyError will have been thrown or status will be
        * CLASS_VERIFIED. We can skip execution of <clinit> altogether if
        * it does not exists AND the superclass is already initialised.
        */
        if ((getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_SUPERCLASS) == 0 ||
            getDWordAt(getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_SUPERCLASS) + INSTANCE_CLASS_STATUS) == CLASS_READY) &&
            getSpecialMethod(thisClass,clinitNameAndType).common_ptr_ == 0) {
                setClassStatus(thisClass,CLASS_READY);
        }
        else {
            TRY {
                pushFrame(RunCustomCodeMethod);
                pushStackAsType(CustomCodeCallbackFunction, (far_ptr)&runClinit);
                pushStackAsType(INSTANCE_CLASS_FAR, thisClass.common_ptr_);
                pushStackAsType(long, 1);
            } CATCH (e) {
                /* Stack overflow */
                setClassStatus(thisClass, CLASS_ERROR);
                THROW(e);
            } END_CATCH
        }
    }
}


/*=========================================================================
* FUNCTION:      runClinit()
* TYPE:          private initialization of a class
* OVERVIEW:      Initialize a class. The Class.runCustomCode frame has
*                already been pushed.
*
*        This function follows the exact steps as documented in
*        the Java virtual machine spec (2ed) 2.17.5, except that
*        Error is used instead of ExceptionInInitializerError
*        because the latter is not defined in CLDC.
*=======================================================================*/
static void runClinit(FRAME_HANDLE_FAR exceptionFrameH) {
    INSTANCE_CLASS_FAR thisClass;
    int state;
    BOOL haveMonitor = FALSE;
    if (exceptionFrameH.common_ptr_ != 0) {
        runClinitException(exceptionFrameH);
        return;
    }

    state = topStackAsType(far_ptr);
    thisClass.common_ptr_ = secondStackAsType(INSTANCE_CLASS_FAR);

    /* The 11 steps as documented in page 53 of the virtual machine spec. */
    switch (state) {
    case 1:
        /* A short cut that'll probably happen 99% of the time.  This class
        * has no monitor, and no one is in the middle of initializing this
        * class.  Since our scheduler is non preemptive, we can just
        * mark the class, without worrying about the monitor or other threads.
        */
        if (!OBJECT_HAS_MONITOR(getDWordAt(thisClass.common_ptr_))
            && getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_INITTHREAD) == 0) {
                goto markClass;
        }

        /* Step 1:  Grab the class monitor so we have exclusive access. */
        {
            OBJECT_FAR of;
            of.common_ptr_ = thisClass.common_ptr_;
            if (monitorEnter(of) != MonitorStatusOwn) {
                /* We've been forced to wait.  When we're awoken, we'll have
                * the lock */

                //topStack = 2;
                setDWordAt(getSP(), 2);

                return;
            } else {
                /* FALL THROUGH.  We have the lock */
            }
        }

    case 2:
        haveMonitor = TRUE;

        if (getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_INITTHREAD) != 0
            && getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_INITTHREAD) != CurrentThread.common_ptr_) {
            /* Step 2:
            * Someone else is initializing this class.  Just wait until
            * a notification.  Of course, we'll have to recheck, since the
            * class could also be notified for other reasons.
            */
            long64 timeout;
            ll_setZero(timeout);
            {
                OBJECT_FAR of;
                of.common_ptr_ = thisClass.common_ptr_;
                monitorWait(of, timeout);
            }
            //topStack = 2;
            setDWordAt(getSP(), 2);
            return;
        }

markClass:
        if (getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_INITTHREAD) == CurrentThread.common_ptr_ ||
            getWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_STATUS) == CLASS_READY) { /* step 4 */
                /* Step 3, Step 4:
                * This thread is already initializing the class, or the class
                * has somehow already become initialized.  We're done.
                */
                if (haveMonitor) {
                    //char *junk;
                    PSTR junk;
                    PSTR_FAR junkFar;
                    OBJECT_FAR of;
                    junkFar.fields_.near_ptr_ = &junk;
                    junkFar.fields_.page_ = getMMUState();
                    of.common_ptr_ = thisClass.common_ptr_;
                    monitorExit(of, &junk);
                }
                popFrame();
                return;
        }
        if (getWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_STATUS) == CLASS_ERROR) {
            /* Step 5:
            * What can we do?
            */
            if (haveMonitor) {
                char *junk;
                OBJECT_FAR of;
                of.common_ptr_ = thisClass.common_ptr_;
                monitorExit(of, &junk);
            }
            popFrame();
            return;
        }

        /* Step 6:
        * Mark that we're about to initialize this class */
        setClassInitialThread(thisClass, CurrentThread);
        if (haveMonitor) {
            char *junk;
            OBJECT_FAR of;
            of.common_ptr_ = thisClass.common_ptr_;
            monitorExit(of, &junk);
            haveMonitor = FALSE;
        }
        /* FALL THROUGH */

    case 3:
        /* Step 7:
        * Initialize the superclass, if necessary */
        if ((getWordAt(thisClass.common_ptr_ + CLASS_ACCESSFLAGS) & ACC_INTERFACE) == 0) {
            INSTANCE_CLASS_FAR superClass;
            superClass.common_ptr_ = getDWordAt(thisClass.common_ptr_ + INSTANCE_CLASS_SUPERCLASS);
            if (superClass.common_ptr_ != 0 && getWordAt(superClass.common_ptr_ + INSTANCE_CLASS_STATUS) != CLASS_READY) {
                //topStack = 4;
                setDWordAt(getSP(), 4);
                initializeClass(superClass);
                return;
            }
        }
        /* FALL THROUGH */

    case 4: {
        /* Step 8:
        * Run the <clinit> method, if the class has one
        */
        METHOD_FAR thisMethod = getSpecialMethod(thisClass, clinitNameAndType);
        if (thisMethod.common_ptr_ != 0) {

            //topStack = 5;
            setDWordAt(getSP(), 5);
            pushFrame(thisMethod);
            return;
        } else {
            /* No <clinit> method. */
            /* FALL THROUGH */
        }
            }
    case 5:
        /* Step 9:
        * Grab the monitor so we can change the flags, and wake up any
        * other thread waiting on us.
        *
        * SHORTCUT: 99% of the time, there is no contention for the class.
        * Since our scheduler is non-preemptive, if there is no contention
        * for this class, we just go ahead and unmark the class, without
        * bothering with the monitor.
        */

        //if (!OBJECT_HAS_MONITOR(thisClass->clazz)) {
        if (!OBJECT_HAS_MONITOR(thisClass.common_ptr_)) {
            goto unmarkClass;
        }

        {
            OBJECT_FAR of;
            of.common_ptr_ = thisClass.common_ptr_;
            if (monitorEnter(of) != MonitorStatusOwn) {
                /* When we wake up, we'll have the monitor */
                setDWordAt(getSP(), 6);
                return;
            } else {
                /* FALL THROUGH */
            }
        }

    case 6:
        haveMonitor = TRUE;
        /* Step 9, cont.
        * Mark the class as initialized. Wake up anyone waiting for the
        * class to be initialized.  Return the monitor.
        */
unmarkClass:
        setClassInitialThread(thisClass, NULL);
        setClassStatus(thisClass, CLASS_READY);
#if ENABLE_JAVA_DEBUGGER
        if (vmDebugReady) {
            CEModPtr cep = GetCEModifier();
            cep->loc.classID = GET_CLASS_DEBUGGERID(&thisClass->clazz);
            cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
            cep->eventKind = JDWP_EventKind_CLASS_PREPARE;
            insertDebugEvent(cep);
        }
#endif /* ENABLE_JAVA_DEBUGGER */
        if (haveMonitor) {
            char *junk;
            OBJECT_FAR of;
            of.common_ptr_ = thisClass.common_ptr_;
            monitorNotify(of, TRUE); /* wakeup everyone */
            monitorExit(of, &junk);
        }
        popFrame();
        return;
        /* Step 10, 11:
        * These handle error conditions that cannot currently be
        * implemented in the KVM.
        */

    default:
        fatalVMError(KVM_MSG_STATIC_INITIALIZER_FAILED);
    }
}

static void runClinitException(FRAME_HANDLE_FAR frameH)
{
    START_TEMPORARY_ROOTS
        void **bottomStack = (void **)(unhand(frameH) + 1); /* transient */
        INSTANCE_CLASS thisClass = bottomStack[1];
        int state = (int)bottomStack[2];
        DECLARE_TEMPORARY_ROOT(THROWABLE_INSTANCE, exception, bottomStack[0]);

        /*
        * Must be:
        *   a. a class initialization during bootstrap (state == 1), or
        *   a. executing either clinit or superclass(es) clinit
        *      (state == 4 || state = 5)
        */
        if (state != 1 && state != 4 && state != 5)
            fatalVMError(KVM_MSG_STATIC_INITIALIZER_FAILED);

        setClassStatus(thisClass, CLASS_ERROR);
        setClassInitialThread(thisClass, NULL);

        /* Handle exception during clinit */
        if (!isAssignableTo((CLASS)(exception->ofClass),(CLASS)JavaLangError)) {

            /* Replace exception with Error, then continue the throwing */
            DECLARE_TEMPORARY_ROOT(THROWABLE_INSTANCE, error,
            (THROWABLE_INSTANCE)instantiate(JavaLangError));
            DECLARE_TEMPORARY_ROOT(STRING_INSTANCE, messageString,
            exception->message);
            char *p = str_buffer;

            /* Create the new message:
            *   Static initializer: <className of throwable> <message>
            */
            strcpy(p, "Static initializer: ");
            p += strlen(p);
            getClassName_inBuffer(&exception->ofClass->clazz, p);
            p += strlen(p);
            if (messageString != NULL) {
                strcpy(p, ": ");
                p += strlen(p);
                getStringContentsSafely(messageString, p,
                    STRINGBUFFERSIZE - (p - str_buffer));
                p += strlen(p);
            }
            error->message = instantiateString(str_buffer, p - str_buffer);
            /* Replace the exception with our new Error, continue throwing */
            *(THROWABLE_INSTANCE *)(unhand(frameH) + 1) = error;

            /* ALTERNATIVE, JLS/JVMS COMPLIANT IMPLEMENTATION FOR THE CODE ABOVE:  */
            /* The code below works correctly if class ExceptionInInitializerError */
            /* is available.  For CLDC 1.1, we will keep on using the earlier code */
            /* located above, since ExceptionInInitializerError is not supported.  */

            /* Replace exception with Error, then continue the throwing */
            /*
            DECLARE_TEMPORARY_ROOT(THROWABLE_INSTANCE, error, NULL);

            TRY {
            raiseException(ExceptionInInitializerError);
            } CATCH(e) {
            if ((CLASS)e->ofClass == getClass(ExceptionInInitializerError)){
            *((THROWABLE_INSTANCE*)&(e->backtrace) + 1) = exception;
            }
            error = e;
            } END_CATCH
            *(THROWABLE_INSTANCE *)(unhand(frameH) + 1) = error;
            */
        }

        /* If any other thread is waiting for this, we should try to wake them
        * up if we can.
        */
        if (OBJECT_HAS_REAL_MONITOR(&thisClass->clazz)) {
            MONITOR monitor = OBJECT_MHC_MONITOR(&thisClass->clazz);
            if (monitor->owner == NULL || monitor->owner == CurrentThread) {
                char *junk;
                monitorEnter((OBJECT)thisClass);
                monitorNotify((OBJECT)thisClass, TRUE); /* wakeup everyone */
                monitorExit((OBJECT)thisClass, &junk);
            }
        }

    END_TEMPORARY_ROOTS
}
