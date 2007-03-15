/* project : javaonzx
   file    : class.h
   purpose : Internal runtime class structures
   author  : valker
*/

#ifndef CLASS_H_INCLUDED
#define CLASS_H_INCLUDED

#include <stdarg.h>
#include "jvm_types.h"
#include "hashtable.h"

extern THROWABLE_INSTANCE_FAR OutOfMemoryObject;

//typedef union cellOrPointer {
//    cell cell;
//    cell *cellp;
//    cell **cellpp;
//    char *charp;
//    char **charpp;
//} cellOrPointer;
typedef far_ptr cellOrPointer;

/* POINTERLIST */
struct pointerListStruct {
  long  length;
  cellOrPointer data[1];
};

typedef union monitorOrHashCode { 
    void *address;          /* low 2 bits MHC_MONITOR,  
                            MHC_SIMPLE_LOCK, 
                            MHC_EXTENDED_LOCK */
    long  hashCode;         /* low 2 bits MHC_UNLOCKED */
} monitorOrHashCode;


#define COMMON_OBJECT_INFO(_type_) \
    _type_ ofClass; /* Pointer to the class of the instance */ \
    monitorOrHashCode mhc;


struct throwableInstanceStruct { 
    COMMON_OBJECT_INFO(INSTANCE_CLASS_FAR)
    STRING_INSTANCE_FAR message;
    ARRAY_FAR   backtrace;
};

/* CLASS */
struct classStruct {
    COMMON_OBJECT_INFO(INSTANCE_CLASS_FAR)

    /* Note that arrays classes have the same packageName as their base
    * class.  the baseName for arrays is [...[L<className>; or
    * [...[<primitive type>, where [...[ indicates the appropriate
    * number of left brackets for the depth */
    UString_FAR packageName;            /* Everything before the final '/' */
    UString_FAR baseName;               /* Everything after the final '/' */
    CLASS_FAR   next;                   /* Next item in this hash table bucket */

    unsigned short accessFlags;     /* Access information */
    unsigned short key;             /* Class key */
};

#define CLASS_OF_CLASS offsetof(struct classStruct, ofClass)
#define CLASS_KEY offsetof(struct classStruct, key)

typedef void (*NativeFuncPtr) (INSTANCE_HANDLE_FAR);


/* INSTANCE_CLASS */
struct instanceClassStruct {
    struct classStruct clazz;       /* common info */

    /* And specific to instance classes */
    INSTANCE_CLASS_FAR superClass;      /* Superclass, unless java.lang.Object */
    CONSTANTPOOL_FAR constPool;         /* Pointer to constant pool */
    FIELDTABLE_FAR  fieldTable;         /* Pointer to instance variable table */
    METHODTABLE_FAR methodTable;        /* Pointer to virtual method table */
    unsigned short* ifaceTable;         /* Pointer to interface table */
    POINTERLIST_FAR staticFields;       /* Holds static fields of the class */
    u2   instSize;                   /* The size of class instances */
    u2 status;                       /* Class readiness status */
    THREAD_FAR initThread;              /* Thread performing class initialization */
    NativeFuncPtr finalizer;            /* Pointer to finalizer */
};

#define INSTANCE_CLASS_STATUS offsetof(struct instanceClassStruct, status)

/* ARRAY_CLASS */
struct arrayClassStruct {
    struct classStruct clazz;       /* Common info */

    /* And stuff specific to an array class */
    union {
        CLASS_FAR elemClass;        /* Element class of object arrays */
        long primType;              /* Element type for primitive arrays */
    } u;
    long itemSize;                  /* Size (bytes) of individual elements */
    /* wants to be GCT_ObjectType rather than */
    /* an int. But the Palm makes enumerations*/
    /* into "short" */
    long gcType;                    /* GCT_ARRAY or GCT_OBJECTARRAY */
    long flags;                     /*   */
};

#define ARRAY_CLASS_CLAZZ offsetof(struct arrayClassStruct, clazz)

/* INSTANCE */
struct instanceStruct {
    COMMON_OBJECT_INFO(INSTANCE_CLASS_FAR)
    union {
//        cell *cellp;
//        cell cell;
        far_ptr ptr;
    } data[1];
};

/* ARRAY */
struct arrayStruct {
    COMMON_OBJECT_INFO(ARRAY_CLASS_FAR)

    cell  length;               /* Number of elements */
    cellOrPointer data[1];  
};



extern NameTypeKey initNameAndType;
extern NameTypeKey clinitNameAndType;
extern NameTypeKey runNameAndType;
extern NameTypeKey mainNameAndType;
extern METHOD_FAR RunCustomCodeMethod;
extern INSTANCE_CLASS_FAR JavaLangOutOfMemoryError; /* java.lang.OutOfMemoryError */

//////////////////////////////////////////////////////////////////////////
// functions declarations

ARRAY_CLASS_FAR getArrayClass(int depth, INSTANCE_CLASS_FAR baseClass, char signCode);
CLASS_FAR getClass(PSTR_FAR name);
PSTR_FAR getClassName(CLASS_FAR clazz);
INSTANCE_FAR instantiate(INSTANCE_CLASS_FAR);
long     arrayItemSize(int arrayType);
char typeCodeToSignature(char typeCode);
void InitializeJavaSystemClasses(void);
CLASS_FAR getRawClass(PSTR_FAR name);
PSTR_FAR  getClassName_inBuffer(CLASS_FAR clazz, PSTR_FAR resultBuffer);

#define RunCustomCodeMethod_MAX_STACK_SIZE 4

/* Class status flags */
#define CLASS_RAW       0 /* this value must be 0 */
#define CLASS_LOADING   1
#define CLASS_LOADED    2
#define CLASS_LINKED    3
#define CLASS_VERIFIED  4
#define CLASS_READY     5
#define CLASS_ERROR    -1

#define ITEM_NewObject_Flag 0x1000
#define ITEM_NewObject_Mask 0x0FFF


#define ARRAY_FLAG_BASE_NOT_LOADED 1

/*=========================================================================
* Sizes for the above structures
*=======================================================================*/

#define SIZEOF_CLASS              sizeof(struct classStruct)
#define SIZEOF_INSTANCE(n)        sizeof(struct instanceStruct)+(n-1)*sizeof(far_ptr) //(StructSizeInCells(instanceStruct)+((n)-1))
#define SIZEOF_ARRAY(n)           sizeof(struct arrayStruct)+(n-1)*sizeof(far_ptr) //(StructSizeInCells(arrayStruct)+((n)-1))

#define SIZEOF_INSTANCE_CLASS     sizeof(struct instanceClassStruct)
#define SIZEOF_ARRAY_CLASS        sizeof(struct arrayClassStruct)

#define SIZEOF_POINTERLIST(n)     sizeof(struct pointerListStruct)+(n-1) * sizeof(far_ptr)
#define SIZEOF_WEAKPOINTERLIST(n) sizeof(struct weakPointerListStruct)+(n-1) * sizeof(far_ptr)

#define SIZEOF_STRING_INSTANCE           SIZEOF_INSTANCE(3)
#define SIZEOF_INTERNED_STRING_INSTANCE  SIZEOF_INSTANCE(4)

//#define IS_ARRAY_CLASS(c) (( ((CLASS)(c))->accessFlags & ACC_ARRAY_CLASS) != 0)
BOOL IS_ARRAY_CLASS(CLASS_FAR clazz);


#endif
