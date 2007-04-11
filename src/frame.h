/* project : javaonzx
   file    : frame.h
   purpose : Definitions for execution frame & exception handling manipulation.
   author  : valker
*/

#ifndef FRAME_H_INCLUDED
#define FRAME_H_INCLUDED

#include "jvm_types.h"
#include "main.h"

#define MAXIMUM_STACK_AND_LOCALS 512

/* STACK */
struct stackStruct {
    STACK_FAR   next;
    u2          size;
    u2          xxunusedxx; /* must be multiple of 4 on all platforms */
    far_ptr     cells[STACKCHUNKSIZE];
};

#define STACKSTRUCT_NEXT offsetof(struct stackStruct,next)
#define STACKSTRUCT_SIZE offsetof(struct stackStruct,size)
#define STACKSTRUCT_CELLS offsetof(struct stackStruct,cells)


/* HANDLER */
struct exceptionHandlerStruct {
    u2 startPC;   /* Start and end program counter indices; these */
    u2 endPC;     /* determine the code range where the handler is valid */
    u2 handlerPC; /* Location that is called upon exception */
    u2 exception; /* Class of the exception (as a constant pool index) */
    /* Note: 0 in 'exception' indicates an 'any' handler */
};

#define HANDLER_STARTPC offsetof(struct exceptionHandlerStruct, startPC)
#define HANDLER_ENDPC offsetof(struct exceptionHandlerStruct, endPC)
#define HANDLER_HANDLERPC offsetof(struct exceptionHandlerStruct, handlerPC)
#define HANDLER_EXCEPTION offsetof(struct exceptionHandlerStruct, exception)

/* HANDLERTABLE */
struct exceptionHandlerTableStruct {
    u2 length;
    struct exceptionHandlerStruct handlers[1];
};

#define HANDLERTABLE_LENGTH offsetof(struct exceptionHandlerTableStruct, length)
#define HANDLERTABLE_HANDLERS offsetof(struct exceptionHandlerTableStruct, handlers)

#define SIZEOF_HANDLER           sizeof(struct exceptionHandlerStruct)
#define SIZEOF_HANDLERTABLE(n) (sizeof(struct exceptionHandlerTableStruct) + (n - 1) * SIZEOF_HANDLER)


/* FRAME (allocated inside execution stacks of threads) */
struct frameStruct {
    FRAME_FAR   previousFp; /* Stores the previous frame pointer */
    BYTES_FAR   previousIp; /* Stores the previous program counter */
    far_ptr     previousSp; /* Stores the previous stack pointer */
    METHOD_FAR  thisMethod; /* Pointer to the method currently under execution */
    STACK_FAR   stack;      /* Stack chunk containing the frame */
    OBJECT_FAR  syncObject; /* Holds monitor object if synchronized method call */
};

#define FRAMESTRUCT_SYNCOBJECT offsetof(struct frameStruct, syncObject)

#define FOR_EACH_HANDLER(__var__, handlerTable) {                                       \
    HANDLER_FAR __first_handler__, __end_handler__, __var__;                             \
    __first_handler__.common_ptr_ = handlerTable.common_ptr_ + HANDLERTABLE_HANDLERS;   \
    __end_handler__.common_ptr_ = __first_handler__.common_ptr_ + getWordAt(handlerTable.common_ptr_) * SIZEOF_HANDLER; \
    for (__var__ = __first_handler__; __var__.common_ptr_ < __end_handler__.common_ptr_; __var__.common_ptr_ += SIZEOF_HANDLER) {

#define END_FOR_EACH_HANDLER } }


extern PSTR_FAR OutOfMemoryError;
extern PSTR_FAR NoClassDefFoundError;
extern PSTR_FAR ClassCircularityError;
extern PSTR_FAR ClassFormatError;
extern PSTR_FAR IncompatibleClassChangeError;
extern PSTR_FAR VerifyError;
extern PSTR_FAR ClassNotFoundException;
extern PSTR_FAR IllegalAccessError;

//void raiseExceptionWithMessage(const char* exceptionClassName, const char* exceptionMessage);
void raiseExceptionWithMessage(PSTR_FAR exceptionClassName, PSTR_FAR exceptionMessage);
void fatalError(const char* errorMessage);
void raiseException(PSTR_FAR exceptionClassName);
const u1 * copyStrToBuffer(u1* buffer, far_ptr str);
void pushFrame(METHOD_FAR thisMethod);



#endif
