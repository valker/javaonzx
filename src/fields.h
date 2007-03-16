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
        struct {
            u1* code;
            HANDLERTABLE_FAR handlers;
            union {
                STACKMAP_FAR pointerMap;
                POINTERLIST_FAR verifierMap;
            } stackMaps;
            unsigned short codeLength;
            unsigned short maxStack; 
            /* frameSize should be here, rather than in the generic part, */
            /* but this gives us better packing of the bytes */
        } java;
        struct { 
            void (*code)(void);
            void *info;
        } native;
    } u;
    long  accessFlags;          /* Access indicators (public/private etc.) */
    INSTANCE_CLASS_FAR ofClass; /* Backpointer to the class owning the field */
    unsigned short frameSize;   /* Method frame size (arguments+local vars) */
    unsigned short argCount;    /* Method argument (parameter) count */
};

/*  FIELD */
struct fieldStruct {
    NameTypeKey nameTypeKey;
    u4         accessFlags;    /* Access indicators (public/private etc.) */
    INSTANCE_CLASS ofClass;     /* Backpointer to the class owning the field */
    union { 
        u2  offset;             /* for dynamic objects */
        far_ptr staticAddress;  /* pointer to static.  */
    } u;
};

#define FIELD_ACCESSFLAGS offsetof(struct fieldStruct, accessFlags)
#define FIELD_U offsetof(struct fieldStruct, u)

struct fieldTableStruct { 
    u2 length;
    struct fieldStruct fields[1];
};


NameTypeKey getNameAndTypeKey(PSTR_FAR name, PSTR_FAR type);
METHOD_FAR getSpecialMethod(INSTANCE_CLASS_FAR thisClass, NameTypeKey key);

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
