/* project : javaonzx
   file    : fields.h
   purpose : Internal runtime structures for storing different kinds of fields
   author  : valker
*/

#ifndef FIELDS_H_INCLUDED
#define FIELDS_H_INCLUDED

/*  METHOD */
struct methodStruct {
    NameTypeKey   nameTypeKey;
    union { 
        struct JavaStruct {
            BYTES_FAR code;
            HANDLERTABLE_FAR handlers;
            union {
                STACKMAP_FAR pointerMap;
                POINTERLIST_FAR verifierMap;
            } stackMaps;
            u2 codeLength;
            u2 maxStack; 
            /* frameSize should be here, rather than in the generic part, */
            /* but this gives us better packing of the bytes */
        } java;
        struct NativeStruct { 
            void (*code)(void);
            void *info;
        } native;
    } u;
    u2  accessFlags;          /* Access indicators (public/private etc.) */
    INSTANCE_CLASS_FAR ofClass; /* Backpointer to the class owning the field */
    u2 frameSize;   /* Method frame size (arguments+local vars) */
    u2 argCount;    /* Method argument (parameter) count */
};

#define SIZEOF_METHOD            sizeof(struct methodStruct)
#define METHOD_NAMETYPEKEY  offsetof(struct methodStruct, nameTypeKey)
#define METHOD_ARGCOUNT  offsetof(struct methodStruct, argCount)
#define METHOD_ACCESSFLAGS  offsetof(struct methodStruct, accessFlags)
#define METHOD_FRAMESIZE  offsetof(struct methodStruct, frameSize)
#define METHOD_U  offsetof(struct methodStruct, u)
#define METHOD_OFCLASS  offsetof(struct methodStruct, ofClass)

#define JAVA_MAXSTACK offsetof(struct JavaStruct, maxStack)
#define JAVA_HANDLERS offsetof(struct JavaStruct, handlers)
#define JAVA_CODE offsetof(struct JavaStruct, code)
#define JAVA_CODELENGTH offsetof(struct JavaStruct, codeLength)
#define JAVA_STACKMAPS offsetof(struct JavaStruct, stackMaps)

#define NATIVE_INFO offsetof(struct NativeStruct, info)
#define NATIVE_CODE offsetof(struct NativeStruct, code)

/*  FIELD */
struct fieldStruct {
    NameTypeKey nameTypeKey;
    u2         accessFlags;     /* Access indicators (public/private etc.) */
    INSTANCE_CLASS_FAR ofClass; /* Backpointer to the class owning the field */
    union { 
        u2  offset;             /* for dynamic objects */
        far_ptr staticAddress;  /* pointer to static.  */
    } u;
};

#define SIZEOF_FIELD sizeof(struct fieldStruct)

#define FIELD_NAMETYPEKEY   offsetof(struct fieldStruct, nameTypeKey)
#define FIELD_ACCESSFLAGS   offsetof(struct fieldStruct, accessFlags)
#define FIELD_U             offsetof(struct fieldStruct, u)
#define FIELD_OFCLASS       offsetof(struct fieldStruct, ofClass)

struct fieldTableStruct { 
    u2 length;
    struct fieldStruct fields[1];
};

#define FIELDTABLE_LENGTH offsetof(struct fieldTableStruct, length)
#define FIELDTABLE_FIELDS offsetof(struct fieldTableStruct, fields)

struct methodTableStruct { 
    u2 length;
    struct methodStruct methods[1];
};

#define SIZEOF_METHODTABLE(n)  \
    (sizeof(struct methodTableStruct) + (n - 1) * SIZEOF_METHOD)

#define SIZEOF_FIELDTABLE(n)   \
    (sizeof(struct fieldTableStruct) + (n - 1) * SIZEOF_FIELD)

#define METHODTABLE_METHODS offsetof(struct methodTableStruct, methods)


NameTypeKey getNameAndTypeKey(PSTR_FAR name, PSTR_FAR type);
METHOD_FAR getSpecialMethod(INSTANCE_CLASS_FAR thisClass, NameTypeKey key);
MethodTypeKey change_MethodSignature_to_Key(CONST_CHAR_HANDLE_FAR, i2 offset, i2 length);
FieldTypeKey change_FieldSignature_to_Key(CONST_CHAR_HANDLE_FAR, i2 offset, i2 length);

#define FIELD_KEY_ARRAY_SHIFT 13
#define MAX_FIELD_KEY_ARRAY_DEPTH 7

#define FOR_EACH_FIELD(__var__, fieldTable) {                           \
    if (fieldTable.common_ptr_ != 0) {                                  \
    FIELD_FAR __first_field__;                                          \
    FIELD_FAR __end_field__;                                            \
    FIELD_FAR __var__;                                                  \
    __first_field__.common_ptr_ = fieldTable.common_ptr_ + offsetof(struct fieldTableStruct,fields); \
    __end_field__.common_ptr_ = __first_field__.common_ptr_ + getWordAt(fieldTable.common_ptr_) * sizeof(struct fieldStruct); \
    for (__var__ = __first_field__; __var__.common_ptr_ < __end_field__.common_ptr_; __var__.common_ptr_ += sizeof(struct fieldStruct)) { 

#define END_FOR_EACH_FIELD } } }

#endif
