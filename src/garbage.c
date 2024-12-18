/* project : javaonzx
   file    : garbage.c
   purpose : memory management
   author  : valker
*/

#include "common.h"
#include "garbage.h"
#include "thread.h"
#include "class.h"
#include "root_code.h"
#include "class.h"
#include "seh.h"
#include "messages.h"
#include "frame.h"

///* VARIABLES */
int TemporaryRootsLength;
int GlobalRootsLength;
int gcInProgress;
cellOrPointer TemporaryRoots[MAXIMUM_TEMPORARY_ROOTS];
cellOrPointer GlobalRoots[MAXIMUM_GLOBAL_ROOTS];
POINTERLIST_FAR         CleanupRoots;

/* EXTERNALS */
void InitializeHeap(void);
//
///*=========================================================================
// * FUNCTION:      garbageCollect
// * TYPE:          public garbage collection function
// * OVERVIEW:      Perform mark-and-sweep garbage collection.
// * INTERFACE:
// *   parameters:  int moreMemory: the amount by which the heap
// *                size should be grown during garbage collection
// *                (this feature is not supported in the mark-and-sweep
// *                collector).
// *   returns:     <nothing>
// *=======================================================================*/
void garbageCollect(i2 moreMemory) {
  (void)moreMemory;
}
//
///*=========================================================================
// * FUNCTION:      mallocHeapObject()
// * TYPE:          public memory allocation operation
// * OVERVIEW:      Allocate a contiguous area of n cells in the dynamic heap.
// * INTERFACE:
// *   parameters:  size: the requested object size in cells,
// *                type: garbage collection type of the object
// *   returns:     pointer to newly allocated area, or
// *                NIL is allocation fails.
// * COMMENTS:      Allocated data area is not initialized, so it
// *                may contain invalid heap references. If you do
// *                not intend to initialize data area right away,
// *                you must use 'callocObject' instead (or otherwise
// *                the garbage collector will get confused next time)!
// *=======================================================================*/
far_ptr mallocHeapObject(u2 size, GCT_ObjectType type) {
  if (size == 0) 
    size = 1;
  if (0/*EXCESSIVE_GARBAGE_COLLECTION*/) {
    garbageCollect(0);
  }
  {
    FPD p,p2;
    p.common_ptr_ = hAlloc(size);
    p2 = p;
    --(p.fields_.near_ptr_);
    setBlockType(p, type, FALSE);
    return p2.common_ptr_;
  }
}
//        
//
//
void InitializeGlobals(void) {
  extern BOOL loadedReflectively;
  loadedReflectively = FALSE;
}
//
//
far_ptr callocObject(u2 size, GCT_ObjectType type) {
  far_ptr result = mallocHeapObject(size, type);
  if (result == 0) {
    THROW(OutOfMemoryObject);
  }
  /* Initialize the area to zero */
  hmemset(result, 0, size);
  return result;
}

far_ptr callocPermanentObject(u2 size) {
    if (size == 0) 
        size = 1;
    if (0/*EXCESSIVE_GARBAGE_COLLECTION*/) {
        garbageCollect(0);
    }
    {
        FPD p,p2;
        p.common_ptr_ = hAlloc(size);
        p2 = p;
        --(p.fields_.near_ptr_);
        setBlockType(p, GCT_NOPOINTERS, TRUE);
        return p2.common_ptr_;
    }
}



///*=========================================================================
// * FUNCTION:      InitializeMemoryManagement()
// * TYPE:          public global operation
// * OVERVIEW:      Initialize the system memory heaps.
// * INTERFACE:
// *   parameters:  <none>
// *   returns:     <nothing>
// *=======================================================================*/
void InitializeMemoryManagement(void) {
  int index;
  gcInProgress = 0;
  InitializeHeap();
  index = 0;
  //GlobalRoots[index++].cellpp = (cell **)&AllThreads;
  //GlobalRoots[index++].cellpp = (cell **)&CleanupRoots;
  GlobalRoots[index++] = AllThreads.common_ptr_;
  GlobalRoots[index++] = CleanupRoots.common_ptr_;

  GlobalRootsLength = index;
  TemporaryRootsLength = 0;
//  CleanupRoots = (POINTERLIST)callocObject(SIZEOF_POINTERLIST(CLEANUP_ROOT_SIZE), GCT_POINTERLIST);
  CleanupRoots.common_ptr_ = callocObject(sizeof(struct pointerListStruct) * CLEANUP_ROOT_SIZE, GCT_POINTERLIST);
}
far_ptr mallocObject(u2 size, GCT_ObjectType type)
{
    far_ptr result = mallocHeapObject(size, type);
    if (result == 0) {
        THROW(OutOfMemoryObject);
    }

    return result;
}
//
far_ptr mallocBytes(u2 size)
{
    return mallocObject(size, GCT_NOPOINTERS);
}

void makeGlobalRoot(far_ptr object)
{
    int index = GlobalRootsLength;
    if (index >= MAXIMUM_GLOBAL_ROOTS) {
        fatalError(KVM_MSG_GLOBAL_ROOT_OVERFLOW);
    }
    GlobalRoots[index] = object;
    GlobalRootsLength = index + 1;
}
