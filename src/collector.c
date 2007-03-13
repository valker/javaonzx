/* project : javaonzx
   file    : collector.c
   purpose : garbage collector
   author  : valker
*/

#include "jvm_types.h"

void* TheHeap;
int  VMHeapSize;               /* Heap size */
extern int RequestedHeapSize;
cell* AllHeapStart;             /* Heap bottom */
cell* CurrentHeap;              /* Same as AllHeapStart*/
cell* CurrentHeapEnd;           /* End of heap */
cell* AllHeapEnd;

/*=========================================================================
 * Heap initialization operations
 *=======================================================================*/
void InitializeHeap(void) {
}
