#include <stddef.h>
#include "hashtable.h"
#include "root_code.h"
#include "class.h"
#include "garbage.h"
#include "fields.h"
#include "frame.h"
#include "messages.h"
//
HASHTABLE_FAR ClassTable;           /* package/base to CLASS */
HASHTABLE_FAR UTFStringTable;       /* char* to unique instance */


PSTR_FAR UStringInfo(PUTF_HASH_ENTRY_FAR hash) {
    PSTR_FAR r;
    r.common_ptr_ = hash.common_ptr_;
    r.common_ptr_ += offsetof(struct UTF_Hash_Entry, string);
    return r;
}
//
//
typedef far_ptr_of(CLASS_FAR) CLASS_FAR_FAR;
///*=========================================================================
//* FUNCTION:      change_Name_to_CLASS, change_Key_to_CLASS
//* OVERVIEW:      Internal function used by internString and internClass.
//*                It maps a C string to a specific location capable of
//*                holding a value.  This location is initially NULL.
//* INTERFACE:
//*   parameters:  table:  A hashtable whose buckets are MAPPING_HASH_ENTRY.
//*                string:  A C string
//*   returns:     A unique location, capable of holding a value, corresponding
//*                to that string.
//*=======================================================================*/
CLASS_FAR change_Name_to_CLASS(UString_FAR packageName, UString_FAR baseName) { 
    HASHTABLE_FAR   table = ClassTable;
    u2    hash;
    u2    index;
    u2    lastKey = 0;
    CLASS_FAR_FAR   clazzPtr;
    CLASS_FAR       clazz;
    u2  length;

    readHmem(&length, baseName.common_ptr_ + UTF_LENGTH, sizeof(length));
    hash = stringHash(UStringInfo(baseName), length) + 37;
    if (packageName.common_ptr_ != 0) { 
        readHmem(&length, packageName.common_ptr_ + UTF_LENGTH, sizeof(length));
        hash += stringHash(UStringInfo(packageName), length) * 3;
    } 
    {
        //index = hash % table->bucketCount;
        u2 bucketCount;
        readHmem(&bucketCount, table.common_ptr_ + offsetof(struct HashTable, bucketCount), sizeof(bucketCount));
        index = hash % bucketCount;
    }

    clazzPtr.common_ptr_ = table.common_ptr_ + HASHTABLE_BUCKET + index * sizeof(PUTF_HASH_ENTRY_FAR);

    for (   readHmem(&clazz, clazzPtr.common_ptr_, sizeof(clazz)); 
            clazz.common_ptr_ != 0;
            readHmem(&clazz, clazz.common_ptr_ + offsetof(struct classStruct, next), sizeof(clazz))) { 
        struct classStruct copy;
        readHmem(&copy, clazz.common_ptr_, sizeof(struct classStruct));
        if (copy.packageName.common_ptr_ == packageName.common_ptr_ && copy.baseName.common_ptr_ == baseName.common_ptr_) { 
            if (0/*EXCESSIVE_GARBAGE_COLLECTION && !ASYNCHRONOUS_NATIVE_FUNCTIONS*/){
                /* We might garbage collect, so we do */
                garbageCollect(0);
            }
            return clazz;
        }
        if (lastKey == 0) { 
            unsigned thisKey = copy.key;//clazz->key;
            int pseudoDepth = thisKey >> FIELD_KEY_ARRAY_SHIFT;
            if (pseudoDepth == 0 || pseudoDepth == MAX_FIELD_KEY_ARRAY_DEPTH) { 
                lastKey = thisKey & 0x1FFF;
            }
        }
    }
    if (getCharAt(UStringInfo(baseName).common_ptr_) == '[') { 
        clazz.common_ptr_ = callocPermanentObject(SIZEOF_ARRAY_CLASS);
    } else { 
        clazz.common_ptr_ = callocPermanentObject(SIZEOF_INSTANCE_CLASS);
    }
    //clazz->next = *clazzPtr;
    {
        CLASS_FAR tmp;
        readHmem(&tmp, clazzPtr.common_ptr_, sizeof(tmp));
        writeHmem(&tmp, clazz.common_ptr_ + offsetof(struct classStruct, next), sizeof(CLASS_FAR));
    }
    
    //*clazzPtr = clazz;
    writeHmem(&clazz, clazzPtr.common_ptr_, sizeof(clazz));

    //clazz->packageName = packageName;
    writeHmem(&packageName, clazz.common_ptr_ + offsetof(struct classStruct, packageName), sizeof(packageName));
    //clazz->baseName = baseName;
    writeHmem(&baseName, clazz.common_ptr_ + offsetof(struct classStruct, baseName), sizeof(baseName));

    /* The caller may change this value to be one of the values with 
    * depth 1 <= depth <= MAX_FIELD_KEY_ARRAY_DEPTH. */
    //clazz->key = lastKey == 0 ? (256 + index) : (lastKey + table->bucketCount);
    {
        u2 bucketCount;
        u2 key;
        readHmem(&bucketCount, table.common_ptr_ + offsetof(struct HashTable, bucketCount), sizeof(bucketCount));
        key = lastKey == 0 ? (256 + index) : (lastKey + bucketCount);
        writeHmem(&key, clazz.common_ptr_ + CLASS_KEY, sizeof(key));
        if (key & ITEM_NewObject_Flag) { 
            fatalError(KVM_MSG_TOO_MANY_CLASS_KEYS);
        }
    }

    {
        //table->count++;
        u2 count;
        readHmem(&count, table.common_ptr_ + offsetof(struct HashTable, count), sizeof(count));
        ++count;
        writeHmem(&count, table.common_ptr_ + offsetof(struct HashTable, count), sizeof(count));
    }
    return clazz;

}

UString_FAR getUStringNear(const char* string) {
    PSTR_FAR fp;
    fp.common_ptr_ = (far_ptr) string;
    return getUString(fp);
}
//
//
///*=========================================================================
//* FUNCTION:      getUString, getUStringX
//* OVERVIEW:      Returns a unique instance of a given C string
//* INTERFACE:
//*   parameters:  string:       Pointer to an array of characters
//*                stringLength: Length of string (implicit for getUString)
//*   returns:     A unique instance.  If strcmp(x,y)==0, then
//*                 getUString(x) == getUString(y), and
//*                 strcmp(UStringInfo(getUString(x)), x) == 0.
//* 
//* Note that getUString actually allows "string"s with embedded NULLs.
//* Two arrays of characters are equal only if they have the same length 
//* and the same characters.
//*=======================================================================*/
//
UString_FAR getUString(PSTR_FAR string) { 
    return getUStringXPtr(string, 0, hstrlen(string.common_ptr_));
}
//
//
typedef far_ptr_of(PUTF_HASH_ENTRY_FAR) PUTF_HASH_ENTRY_FAR_FAR;
//
UString_FAR getUStringX(CONST_CHAR_HANDLE_FAR nameH, u2 offset, u2 length) {
    PSTR_FAR start;
    readHmem(&start, nameH.common_ptr_, sizeof(start));
    return getUStringXPtr(start, offset, length);
}
//
//
UString_FAR getUStringXPtr(PSTR_FAR start, u2 offset, u2 stringLength)
{
    PSTR_FAR string;
    HASHTABLE_FAR table;
    u2 index;
    PUTF_HASH_ENTRY_FAR_FAR bucketPtr;
    PUTF_HASH_ENTRY_FAR bucket;
    u2 bucketCount;

    string.common_ptr_ = start.common_ptr_ + offset; 
    table = UTFStringTable;
    /* The bucket in the hash table */
    readHmem(&bucketCount, table.common_ptr_ + offsetof(struct HashTable, bucketCount),sizeof(bucketCount));
    index =  stringHash(string, stringLength) % bucketCount;
    //PUTF_HASH_ENTRY_FAR *bucketPtr = &table->bucket[index];
    bucketPtr.common_ptr_ = table.common_ptr_+ HASHTABLE_BUCKET + index*sizeof(PUTF_HASH_ENTRY_FAR);
    /* Search the bucket for the corresponding string. */
    for (   readHmem(&bucket,bucketPtr.common_ptr_,sizeof(bucket));
            bucket.common_ptr_ != 0;
            readHmem(&bucket,bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, next), sizeof(bucket))) { 
        //const char *key = bucket->string;
        PSTR_FAR key;
        int length;
        readHmem(&length, bucket.common_ptr_ + UTF_LENGTH, sizeof(length));
        key.common_ptr_ = bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, string);
        if (length == stringLength) { 
            if ((key.common_ptr_ == string.common_ptr_) ||
                (/*key[0] == string[0] && */hmemcmp(key.common_ptr_, string.common_ptr_, length) == 0)) {
                    if (1/*EXCESSIVE_GARBAGE_COLLECTION && !ASYNCHRONOUS_NATIVE_FUNCTIONS*/){
                        /* We'd garbage collect if it weren't found. . . */
                        garbageCollect(0);
                    } 
                    return bucket;
            }
        }
    }

    /* We have to create a new bucket.  Note that the string is embedded
    * into the bucket.  We always append a '\0' to the string so that if
    * the thing we were passed is a C String, it will continue to be a C
    * String */
    bucket.common_ptr_ = callocPermanentObject(SIZEOF_UTF_HASH_ENTRY(stringLength));

    /* Install the new item into the hash table.
    * The allocation above may cause string to no longer be valid 
    */

    {
        // 2 alternatives
        //bucket->next = *bucketPtr;
        PUTF_HASH_ENTRY_FAR next;
        readHmem(&next,bucketPtr.common_ptr_,sizeof(next));
        writeHmem(&next,bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, next), sizeof(next));
    }
    {
        //hmemcpy(bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, next), bucketPtr.common_ptr_,sizeof(next));
    }
    //memcpy((char *)bucket->string, unhand(nameH) + offset, stringLength);
    hmemcpy(bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, string),
            start.common_ptr_,
            stringLength);
    {
        //bucket->string[stringLength] = '\0';
        const u1 tmp = '\0';
        writeHmem(&tmp, bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, string) + stringLength, sizeof(tmp));
    }
    /* Give the item a key that uniquely represents it.  On purpose, we assign
    * a key that makes it easy for us to find the right bucket.
    * (key % table->bucketCount) = index.
    */
    {
        PUTF_HASH_ENTRY_FAR next;
        readHmem(&next, bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, next), sizeof(next));
        if (next.common_ptr_ == 0) { 
            //bucket->key = table->bucketCount + index;
            u2 key;
            readHmem(&key, table.common_ptr_ + offsetof(struct HashTable, bucketCount), sizeof(key));
            key += index;
            writeHmem(&key, bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, key), sizeof(key));
        } else { 
            //u2 nextKey = table->bucketCount + bucket->next->key;
            u2 nextKey;
            u2 bucketNextKey;
            readHmem(&nextKey, table.common_ptr_ + offsetof(struct HashTable, bucketCount), sizeof(nextKey));
            readHmem(&bucketNextKey, next.common_ptr_ + offsetof(struct UTF_Hash_Entry, key), sizeof(bucketNextKey));
            nextKey += bucketNextKey;
//            if (nextKey >= 0x10000) { 
//                fatalError(KVM_MSG_TOO_MANY_NAMETABLE_KEYS);
//            }

            //bucket->key = (unsigned short)nextKey;
            writeHmem(&nextKey, bucket.common_ptr_ + offsetof(struct UTF_Hash_Entry, key), sizeof(nextKey));
        }

    }

    //bucket->length = stringLength;
    writeHmem(&stringLength, bucket.common_ptr_ + UTF_LENGTH, sizeof(stringLength));

    //*bucketPtr = bucket;
    writeHmem(&bucket, bucketPtr.common_ptr_, sizeof(bucket));


    /* Increment the count, in case we need this information */
    //table->count++;
    {
        u2 count;
        readHmem(&count, table.common_ptr_ + offsetof(struct HashTable, count), sizeof(count));
        ++count;
        writeHmem(&count, table.common_ptr_ + offsetof(struct HashTable, count), sizeof(count));
    }

    /* Return the string */
    return bucket;
}
