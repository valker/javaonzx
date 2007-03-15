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


CLASS_FAR getRawClassX(CONST_CHAR_HANDLE_FAR nameH, int offset, int length)
{
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

CLASS_FAR
getClassX(CONST_CHAR_HANDLE_FAR nameH, int offset, int length)
{
    CLASS_FAR clazz = getRawClassX(nameH, offset, length);
    if (!IS_ARRAY_CLASS(clazz)) {
        short status;
        INSTANCE_CLASS_FAR icf;
        readHmem(&status, clazz.common_ptr_ + INSTANCE_CLASS_STATUS, sizeof(status));
        icf.common_ptr_ = clazz.common_ptr_;
        if (/*((INSTANCE_CLASS)clazz)->status*/status == CLASS_RAW) {
            loadClassfile(icf, TRUE);
        } else if (/*((INSTANCE_CLASS)clazz)->status*/status == CLASS_ERROR) {
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

void InitializeJavaSystemClasses(void)
{
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
        copy.u.java.code[0] = CUSTOMCODE;
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
    makeGlobalRoot((cell **)&OutOfMemoryObject);
    StackOverflowObject = OutOfMemoryObject;
    /* If we had full JLS/JVMS error classes:
    StackOverflowObject =
    (THROWABLE_INSTANCE)instantiate(
    (INSTANCE_CLASS)getClass(StackOverflowError));
    */
    makeGlobalRoot((cell **)&StackOverflowObject);
}


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
                long gcType = GCT_OBJECTARRAY;
                writeHmem((void*)&gcType,clazz.common_ptr_ + offsetof(struct arrayClassStruct, gcType), sizeof(gcType));
            }
            //clazz->itemSize = arrayItemSize(T_REFERENCE);
            {
                long itemSize = arrayItemSize(T_REFERENCE);
                writeHmem((void*)&itemSize,clazz.common_ptr_ + offsetof(struct arrayClassStruct, itemSize), sizeof(itemSize));
            }

            if (isPrimitiveBase) {
                //clazz->clazz.accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC | ACC_ARRAY_CLASS;
                unsigned short accessFlags = ACC_FINAL | ACC_ABSTRACT | ACC_PUBLIC | ACC_ARRAY_CLASS;
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

long arrayItemSize(int arrayType)
{
    long itemSize;

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
