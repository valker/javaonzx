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


NameTypeKey getNameAndTypeKey(PSTR_FAR name, PSTR_FAR type);
METHOD_FAR getSpecialMethod(INSTANCE_CLASS_FAR thisClass, NameTypeKey key);

#define FIELD_KEY_ARRAY_SHIFT 13
#define MAX_FIELD_KEY_ARRAY_DEPTH 7

#endif
