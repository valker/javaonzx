/* project : javaonzx
   file    : frame.h
   purpose : Definitions for execution frame & exception handling manipulation.
   author  : valker
*/

#ifndef FRAME_H_INCLUDED
#define FRAME_H_INCLUDED

#include "jvm_types.h"

#define MAXIMUM_STACK_AND_LOCALS 512

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

//void raiseExceptionWithMessage(const char* exceptionClassName, const char* exceptionMessage);
void raiseExceptionWithMessage(PSTR_FAR exceptionClassName, PSTR_FAR exceptionMessage);
void fatalError(const char* errorMessage);
void raiseException(PSTR_FAR exceptionClassName);

#endif
