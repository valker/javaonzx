/* project : javaonzx
   file    : hashtable.h
   purpose : internal hashtables
   author  : valker
*/

#ifndef HASHTABLE_H_INCLUDED
#define HASHTABLE_H_INCLUDED

#include "jvm_types.h"

#define SIZEOF_UTF_HASH_ENTRY(n) (offsetof(struct UTF_Hash_Entry, string) + n + 1)


typedef struct UTF_Hash_Entry*  PUTF_HASH_ENTRY;
typedef far_ptr_of(PUTF_HASH_ENTRY) PUTF_HASH_ENTRY_FAR;

typedef PUTF_HASH_ENTRY_FAR UString_FAR;

/* The declaration of one bucket that holds unique instances of UTF strings
*/
typedef struct UTF_Hash_Entry { 
    PUTF_HASH_ENTRY_FAR next;
    u2 length;
    u2 key;
    u1 string[1];          /* The characters of the string */
};

#define UTF_LENGTH offsetof(struct UTF_Hash_Entry, length)
#define UTF_KEY offsetof(struct UTF_Hash_Entry, key)

/* The declaration of a hashtable.  We make the buckets fairly
* generic.
*/
typedef struct HashTable {
    u2 bucketCount;     /* Number of buckets */
    u2 count;           /* Number of total items in the table */
    PUTF_HASH_ENTRY_FAR bucket[1];
} *HASHTABLE;

#define HASHTABLE_BUCKETCOUNT offsetof(struct HashTable, bucketCount)
#define HASHTABLE_COUNT offsetof(struct HashTable, count)
#define HASHTABLE_BUCKET offsetof(struct HashTable, bucket)

typedef far_ptr_of(HASHTABLE) HASHTABLE_FAR;


UString_FAR getUStringX(CONST_CHAR_HANDLE_FAR nameH, u2 offset, u2 length);
UString_FAR getUStringXPtr(PSTR_FAR nameH, u2 offset, u2 length);
UString_FAR getUString(PSTR_FAR string);
UString_FAR getUStringNear(const char* string);

CLASS_FAR change_Name_to_CLASS(UString_FAR package, UString_FAR base);

//#define UStringInfo(str) str->string
PSTR_FAR UStringInfo(PUTF_HASH_ENTRY_FAR hash);

/* Hashtable containing all the classes in the system */
extern HASHTABLE_FAR ClassTable;
non_banked u2 stringHash(PSTR_FAR s, i2 length);
INTERNED_STRING_INSTANCE_FAR internString(PSTR_FAR string, u2 length);
NameKey change_Name_to_Key(CONST_CHAR_HANDLE_FAR, u2 offset, u2 length);
/* Convert utf8 to unicode */
u2 utf2unicode(PSTR_FAR* utf);

u2 utfStringLength(PSTR_FAR utfstring, u2 length);

#endif
