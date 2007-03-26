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
#include "main.h"
#include "zxfile.h"
#include "native.h"

const ClassNativeImplementationType nativeImplementations[] = {0};


u1 class_image[]={
#include "test_class.h"
};
//
//
//
static struct throwableScopeStruct ThrowableScopeStruct = {
   /* env =           */ 0,
   /* throwable =     */ 0,
   /* tmpRootsCount = */ 0,
   /* outer =         */ 0
};
THROWABLE_SCOPE ThrowableScope = &ThrowableScopeStruct;
void* VMScope = 0;
int   VMExitCode = 0;
//
//
char str_buffer[STRINGBUFFERSIZE];
//
//
//
void fatalVMError(const char* p) {
  VM_EXIT(128);
  (void)p;
}

/*=========================================================================
* FUNCTION:      alertUser()
* TYPE:          error handling operation
* OVERVIEW:      Show an alert dialog to the user and wait for
*                confirmation before continuing execution.
* INTERFACE:
*   parameters:  message string
*   returns:     <nothing>
*=======================================================================*/

void AlertUser(const char* message)
{
    //fprintf(stderr, "ALERT: %s\n", message);
}


//
void InitializeGlobals(void);
void InitializeMemoryManagement(void);
//
void initEmulator(void);

typedef void(non_banked *VoidFunc)(void);

void softReset(void) {
    //_opc(0x0e); // LD C,n
    //_opc(0x00);

    //_opc(0x3e); // LD A,n
    //_opc(0x00);

    //_opc(0xcd); // CALL nn
    //_opc(0x13);
    //_opc(0x3d);

    *(u2*)0x5CC8 = 80;
    *(u2*)0x5CAF = 0xFF;
}

void initDisk(void) {
    _opc(0x0e); // LD C,n
    _opc(0x18);

    _opc(0xcd); // CALL nn
    _opc(0x13);
    _opc(0x3d);
}

void initTrDos(void) {
    //VoidFunc v = (VoidFunc) 0x3D21;
    *(u2*)0x5C4F = 0x5D25;
    (*(VoidFunc)0x3D21)();

    softReset();
    initDisk();
}

void main(void)
{
    // 0 0
    // 1 1
    // 2 3
    // 3 4
    // 4 6
    // 6 7
    static const u1 pages[] = {3,4};
    initTrDos();
  // logical pages
#ifdef ZX
#pragma memory=constseg(ROOT_CONST)
#endif
#ifdef ZX
#pragma memory=default
#endif
  

#ifndef ZX
    initEmulator();
#endif
    hmemInit(&pages[0], sizeof(pages));
    TRY {
      VM_START {
//        InitializeGlobals();
//        InitializeMemoryManagement();
//        InitializeJavaSystemClasses();
      } VM_FINISH(value) {
      } VM_END_FINISH
    } CATCH(e) {
        (void)e;
    } END_CATCH
}

#ifdef ZX
i2 getc(FILE * File)
{
    return 0;
}
i2 fseek(FILE * File, i2 Offset, i1 Origin)
{
    return 0;
}
i2 fread(void * DstBuf, u1 ElementSize, u2 Count, FILE * File) {
    return 0;
}
u2 ftell(FILE * _File) {
    return 0;
}

#endif