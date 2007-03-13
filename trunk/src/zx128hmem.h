/* project : javaonzx
   file    : zx128hmem.h
   purpose : definitions of extended memory management functions
   author  : valentin pimenov
*/

#ifndef ZX128HMEM_INCLUDED_
#define ZX128HMEM_INCLUDED_

/*=========================================================================
 * Garbage collection types of objects (GCT_* types)
 *=========================================================================
 * These types are used for instructing the garbage collector to
 * scan the data fields of objects correctly upon garbage collection.
 * Since two low-end bits of the first header field are used for
 * flags, we don't use these in type declarations.
 *=======================================================================*/
typedef enum {
  GCT_FREE = 0,
  GCT_END,
  /*  Objects which have no pointers inside them */
  /*  (can be ignored safely during garbage collection) */
  GCT_NOPOINTERS,
  /*  Java-level objects which may have mutable pointers inside them */
  GCT_INSTANCE,
  GCT_ARRAY,
  GCT_OBJECTARRAY,
  /* Only if we have static roots */
  GCT_METHODTABLE,
  /*  Internal VM objects which may have mutable pointers inside them */
  GCT_POINTERLIST,
  GCT_EXECSTACK,
  GCT_THREAD,
  GCT_MONITOR,
  /* A weak pointer list gets marked/copied after all other objects */
  GCT_WEAKPOINTERLIST,
  /* Added for java.lang.ref.WeakReference support in CLDC 1.1 */
  GCT_WEAKREFERENCE
} GCT_ObjectType;

typedef unsigned long far_ptr;

typedef union {
    far_ptr ptr_;
    struct {
        u2 offset_;
        u1 log_page_;
        u1 unused_;
    } structured_;
} far_ptr_conv;

typedef struct {
  BOOL            mark_:1;
  GCT_ObjectType  type_:7;
  u2              size_;
} HighMemoryDescriptor;

#endif