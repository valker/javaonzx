/* project : javaonzx
   file    : main.c
   purpose : main module of javaonzx jvm
   author  : valker
*/

#ifdef ZX
#include <intrz80.h>
#endif
#include <stddef.h>
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
#include "frame.h"
#include "loader.h"
#include "thread.h"

const ClassNativeImplementationType nativeImplementations[] = {0};

int func(char d)
{
  char a;
  char b;
  char c;
  if(d == 0) {
    return 0;
  } else {
    a = func(d - 1);
    b = d + 1;
    c = a + b;
    return a + b + c;
  }
}

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

#ifndef ZX
    void _opc(u1 b){}
#endif

void softReset(void) {
    //_opc(0x0e); // LD C,n
    //_opc(0x00);
}

void initTrDos(void) {
    ////VoidFunc v = (VoidFunc) 0x3D21;
    //*(u2*)0x5C4F = 0x5D25;
    //(*(VoidFunc)0x3D21)();

    //softReset();
    //initDisk();
    //initDrive(0);
    //initDisk();
}

static INSTANCE_CLASS_FAR loadMainClass(PSTR_FAR className)
{
    /* Make sure this is not an array class or class is not null */
    if ((getCharAt(className.common_ptr_) == '[') || (hstrcmp(className.common_ptr_, address_24_of("")) == 0)) {
        raiseExceptionWithMessage(NoClassDefFoundError, className);
    }

    /* Replace all the '.' characters in the class name with '/' chars */
    replaceLetters(className, '.', '/');
    {
        //return (INSTANCE_CLASS)getClass(className);
        INSTANCE_CLASS_FAR icf;
        icf.common_ptr_ = getClass(className).common_ptr_;
        return icf;
    }
}

/*=========================================================================
 * FUNCTION:      readCommandLineArguments()
 * TYPE:          constructor (kind of)
 * OVERVIEW:      Read the extra command line arguments provided
 *                by the user and construct a string array containing
 *                the arguments. Note: before executing 'main' the
 *                string array must be pushed onto the operand stack.
 * INTERFACE:
 *   parameters:  command line parameters
 *   returns:     string array containing the command line arguments
 *=======================================================================*/
static ARRAY_FAR readCommandLineArguments(u2 argc, PSTR_FAR argv[]) {
    /* Get the number of command line arguments */
    ARRAY_CLASS_FAR arrayClass = getArrayClass(1, JavaLangString, 0);
    ARRAY_FAR stringArray;
    u2 numberOfArguments = argc;
    u2 argCount = (numberOfArguments > 0) ? numberOfArguments : 0;
    u2 i;

    /* Create a string array for the arguments */
    START_TEMPORARY_ROOTS
        IS_TEMPORARY_ROOT(stringArray, instantiateArray(arrayClass, argCount));
        for (i = 0; i < numberOfArguments; i++) {
            //stringArray->data[i].cellp =
            //    (cell *)instantiateString(argv[i], strlen(argv[i]));
            const STRING_INSTANCE_FAR string = instantiateString(argv[i], hstrlen(argv[i].common_ptr_));
            setDWordAt(stringArray.common_ptr_ + ARRAYSTRUCT_DATA, string.common_ptr_);
        }
    END_TEMPORARY_ROOTS
    return stringArray;
}


void main(void)
{
    INSTANCE_CLASS_FAR mainClass = {0};

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
        InitializeGlobals();
        InitializeMemoryManagement();
        InitializeJavaSystemClasses();
            /* Load the main application class */
            /* If loading fails, we get a C level exception */
            /* and control is transferred to the CATCH block below */
        {
            PSTR_FAR argv[2];
            int argc = COUNTOF(argv);
            argv[0].common_ptr_ = address_24_of(&"someClass");
            argv[1].common_ptr_ = address_24_of(&"param");
            {
                mainClass = loadMainClass(argv[0]);
            }


            {
                ARRAY_FAR arguments;
                /* Parse command line arguments */
                arguments = readCommandLineArguments(argc - 1, argv + 1);

                /* Now that we have a main class, initialize */
                /* the multithreading system */
                InitializeThreading(mainClass, arguments);
            }
        }

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