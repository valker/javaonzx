/* project : javaonzx
   file    : main.c
   purpose : main module of javaonzx jvm
   author  : valker
*/

#ifdef ZX
#include <intrz80.h>
#endif
#include <string.h>
#include "common.h"
#include "zx128hmem.h"
#include "jvm_types.h"
#include "seh.h"
#include "garbage.h"
#include "root_code.h"
#include "class.h"

int RequestedHeapSize;

u1 class_image[]={
#include "test_class.h"
};


void initSystemClasses(void) {
}

static struct throwableScopeStruct ThrowableScopeStruct = {
   /* env =           */ 0,
   /* throwable =     */ 0,
   /* tmpRootsCount = */ 0,
   /* outer =         */ 0
};
THROWABLE_SCOPE ThrowableScope = &ThrowableScopeStruct;
void* VMScope = NULL;
int   VMExitCode = 0;


/* Shared string buffer that is used internally by the VM */
/* NOTE: STRINGBUFFERSIZE is defined in main.h */
char str_buffer[STRINGBUFFERSIZE];



void fatalVMError(const char* p) {
  VM_EXIT(128);
}

void InitializeGlobals(void);
void InitializeMemoryManagement(void);

void initEmulator(void);

void main(void)
{
  // logical pages
#ifdef ZX
#pragma memory=constseg(ROOT_CONST)
#endif
  static const u1 pages[] = {2,3,4};
#ifdef ZX
#pragma memory=default
#endif
  

#ifndef ZX
  initEmulator();
#endif

  RequestedHeapSize = 0x4000 - 2 * sizeof(HighMemoryDescriptor);
  hmemInit(&pages[0], sizeof(pages));
  initSystemClasses();
  {
    TRY {
      VM_START {
        InitializeGlobals();
        InitializeMemoryManagement();
        InitializeJavaSystemClasses();
      } VM_FINISH(value) {
      } VM_END_FINISH
    } CATCH(e) {
    } END_CATCH
  }
}