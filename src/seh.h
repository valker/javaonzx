/* project : javaonzx
   file    : seh.h
   purpose : structured exception handling
   author  : valker
   todo    : add support for far callbacks
*/

#ifndef SEH_H_INCLUDED
#define SEH_H_INCLUDED

#include <setjmp.h>
#include "jvm_types.h"

/*
 * This struct models the nesting of try/catch scopes.
 */
struct throwableScopeStruct {
    /*
     * State relevant to the platform dependent mechanism used to
     * implement the flow of control (e.g. setjmp/longjmp).
     */
    jmp_buf*   env;
    /*
     * A THROWABLE_INSTANCE object.
     */
    THROWABLE_INSTANCE_FAR throwable;
    /*
     * The number of temporary roots at the point of TRY.
     */
    int     tmpRootsCount;
    /*
     * The enclosing try/catch (if any).
     */
    struct throwableScopeStruct* outer;
};

typedef struct throwableScopeStruct* THROWABLE_SCOPE;

#define TRY                                                    \
    {                                                          \
        struct throwableScopeStruct __scope__;                 \
        int __state__;                                         \
        jmp_buf __env__;                                       \
        __scope__.outer = ThrowableScope;                      \
        ThrowableScope = &__scope__;                           \
        ThrowableScope->env = &__env__;                        \
        ThrowableScope->tmpRootsCount = TemporaryRootsLength;  \
        if ((__state__ = setjmp(__env__)) == 0) {

/*
 * Any non-null THROWABLE_INSTANCE passed into a CATCH clause
 * is protected as a temporary root.
 */
#define CATCH(__throwable__)                                   \
        }                                                      \
        ThrowableScope = __scope__.outer;                      \
        TemporaryRootsLength = __scope__.tmpRootsCount;        \
        if (__state__ != 0) {                                  \
            START_TEMPORARY_ROOTS                              \
                 DECLARE_TEMPORARY_ROOT(/*THROWABLE_INSTANCE*/THROWABLE_INSTANCE_FAR,    \
                     __throwable__,__scope__.throwable);

#define END_CATCH                                              \
            END_TEMPORARY_ROOTS                                \
        }                                                      \
    }

/*
 * This macro is required for jumping out of a CATCH block with a goto.
 * This is used in FastInterpret so that the interpreter loop is re-entered
 * with a fresh TRY statement.
 */
#define END_CATCH_AND_GOTO(label)                              \
            END_TEMPORARY_ROOTS                                \
            goto label;                                        \
        }                                                      \
    }

void fatalVMError(const char* p);


#define THROW(__throwable__)                                   \
    {                                                          \
        THROWABLE_INSTANCE_FAR __t__ = __throwable__;          \
        if (__t__.common_ptr_ == 0)                            \
            fatalVMError("THROW called with NULL");            \
        ThrowableScope->throwable = __t__;                     \
        longjmp(*((jmp_buf*)ThrowableScope->env),1);           \
    }



/*
 * This is a non-nesting use of setjmp/longjmp used solely for the purpose
 * of exiting the VM in a clean way. By separating it out from the
 * exception mechanism above, we don't need to worry about whether or not
 * we are throwing an exception or exiting the VM in an CATCH block.
 */

#define VM_START                                               \
    {                                                          \
        jmp_buf __env__;                                       \
        VMScope = &(__env__);                                  \
        if (setjmp(__env__) == 0) {

#define VM_EXIT(__code__)                                      \
        VMExitCode = __code__;                                 \
        longjmp(*((jmp_buf*)VMScope),1)

#define VM_FINISH(__code__)                                    \
        } else {                                               \
            int __code__ = VMExitCode;

#define VM_END_FINISH                                          \
        }                                                      \
    }

extern void* VMScope;
extern int   VMExitCode;
extern THROWABLE_SCOPE ThrowableScope;

#ifndef FATAL_ERROR_EXIT_CODE
#define FATAL_ERROR_EXIT_CODE 127
#endif

#ifndef UNCAUGHT_EXCEPTION_EXIT_CODE
#define UNCAUGHT_EXCEPTION_EXIT_CODE 128
#endif


#endif
