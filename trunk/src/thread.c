/* project : javaonzx
   file    : thread.c
   purpose : threading
   author  : valker
*/

#ifdef ZX
#include <intrz80.h>
#endif
#include <stddef.h>
#include "jvm_types.h"
#include "thread.h"
#include "garbage.h"
#include "root_code.h"
#include "main.h"
#include "interpret.h"
#include "frame.h"
#include "fields.h"
#include "messages.h"
#include "pool.h"

THREAD_FAR AllThreads;      /* List of all threads */

static THREAD_FAR BuildThread(JAVATHREAD_FAR* javaThreadH);


/*=========================================================================
 * FUNCTION:      InitializeThreading()
 * TYPE:          public global operation
 * OVERVIEW:      Create the first low-level system thread and
 *                initialize it properly.  Initialize VM registers
 *                accordingly.
 * INTERFACE:
 *   parameters:  method: The main(String[]) method to call
 *                arguments:  String[] argument
 *   returns:     <nothing>
 *=======================================================================*/

static void
initInitialThreadBehaviorFromThread(FRAME_HANDLE_FAR);

void InitializeThreading(INSTANCE_CLASS_FAR mainClass, ARRAY_FAR argumentsArg) {
    JAVATHREAD_FAR jtf;
    jtf.common_ptr_ = instantiate(JavaLangThread).common_ptr_;
    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(ARRAY_FAR, arguments, argumentsArg);
        DECLARE_TEMPORARY_ROOT(JAVATHREAD_FAR, javaThread, jtf);

        u2 unused; /* Needed for creating name char array */
        //makeGlobalRoot((cell **)&MainThread);
        makeGlobalRoot(address_24_of(&MainThread));
        MainThread.common_ptr_ = 0;
        MonitorCache.common_ptr_ = 0;
//        makeGlobalRoot((cell **)&CurrentThread);
//        makeGlobalRoot((cell **)&RunnableThreads);
//        makeGlobalRoot((cell **)&TimerQueue);
        makeGlobalRoot(address_24_of(&CurrentThread));
        makeGlobalRoot(address_24_of(&RunnableThreads));
        makeGlobalRoot(address_24_of(&TimerQueue));

        /* Initialize the field of the Java-level thread structure */
        //javaThread->priority = 5;
        setDWordAt(javaThread.common_ptr_ + JAVATHREADSTRUCT_PRIORITY, 5);

        /* Initialize the name of the system thread (since CLDC 1.1) */
        //javaThread->name = createCharArray("Thread-0", 8, &unused, FALSE);
        {
            PSTR_FAR name;
            name.common_ptr_ = address_24_of(&"Thread-0");
            setDWordAt(javaThread.common_ptr_ + JAVATHREADSTRUCT_NAME, createCharArray(name, 8, &unused, FALSE).common_ptr_);
        }

        MainThread = BuildThread(&javaThread);

        /* AllThreads is initialized to NULL by the garbage collector.
         *
         * Ensure that the thread list is properly initialized
         * and set mainThread as the active (CurrentThread) thread
         */
        //MainThread->nextThread = NIL;
        setDWordAt(MainThread.common_ptr_ + THREADQUEUE_NEXTTHREAD, 0);
        AliveThreadCount = 1;
        Timeslice = BASETIMESLICE;
        //MainThread->state = THREAD_ACTIVE;
        setCharAt(MainThread.common_ptr_ + THREADQUEUE_STATE, THREAD_ACTIVE);

        /* Initialize VM registers */
        CurrentThread = MainThread;
        RunnableThreads.common_ptr_ = 0;
        TimerQueue.common_ptr_ = 0;


        //setSP((MainThread->stack->cells - 1));
        setSP((getDWordAt(MainThread.common_ptr_ + THREADQUEUE_STACK) + STACKSTRUCT_CELLS - sizeof(far_ptr)));
        setFP(0);
        setIP(1/*KILLTHREAD*/);

        /* We can't create a frame consisting of "method", since its class
         * has not yet been initialized, (and this would mess up the
         * garbage collector).  So we set up a pseudo-frame, and arrange
         * for the interpreter to do the work for us.
         */
        pushFrame(RunCustomCodeMethod);
        pushStackAsType(CustomCodeCallbackFunction, (far_ptr)&initInitialThreadBehaviorFromThread);
        /* We want to push method, but that would confuse the GC, since
         * method is a pointer into the middle of a heap object.  Only
         * heap objects, and things that are clearly not on the heap, can
         * be pushed onto the stack.
         */
        pushStackAsType(INSTANCE_CLASS_FAR, mainClass.common_ptr_);
        pushStackAsType(ARRAY_FAR, arguments.common_ptr_);
        initializeClass(mainClass);
    END_TEMPORARY_ROOTS
}


/*=========================================================================
 * FUNCTION:      BuildThread()
 * TYPE:          public global operation
 * OVERVIEW:      Build a new VM-level thread by instantiating and
 *                initializing the necessary structures.
 * INTERFACE:
 *   returns:     a new thread instance
 * NOTE:          The created thread is an internal structure that does
 *                not map one-to-one with Java class 'Thread.class'.
 *                One-to-one mapping is implemented by structure
 *                JAVATHREAD.
 *=======================================================================*/

static THREAD_FAR BuildThread(JAVATHREAD_FAR* javaThreadH) {
    THREAD_FAR newThread;
    JAVATHREAD_FAR javaThread;
    THREAD_FAR ntx;
    ntx.common_ptr_ = callocObject(SIZEOF_THREAD, GCT_THREAD);
    START_TEMPORARY_ROOTS

        DECLARE_TEMPORARY_ROOT(THREAD_FAR, newThreadX, ntx);
        STACK_FAR newStack;
        newStack.common_ptr_ = callocObject(sizeof(struct stackStruct)/* / CELL*/, GCT_EXECSTACK);
        /* newStack->next = NULL;  Already zero from calloc */
        //newStack->size = STACKCHUNKSIZE;
        setWordAt(newStack.common_ptr_ + STACKSTRUCT_SIZE, STACKCHUNKSIZE);
        //newThreadX->stack = newStack;
        setDWordAt(newThreadX.common_ptr_ + THREADQUEUE_STACK, newStack.common_ptr_);

        newThread = newThreadX;
    END_TEMPORARY_ROOTS

    /* Time slice will be initialized to default value */
    //newThread->timeslice = BASETIMESLICE;
    setDWordAt(newThread.common_ptr_ + THREADQUEUE_TIMESLICE, BASETIMESLICE);

    /* Link the THREAD to the JAVATHREAD */
    javaThread = unhand(javaThreadH);
    //newThread->javaThread = javaThread;
    setDWordAt(newThread.common_ptr_ + THREADQUEUE_JAVATHREAD, javaThread.common_ptr_);
    //javaThread->VMthread = newThread;
    setDWordAt(javaThread.common_ptr_ + JAVATHREADSTRUCT_VMTHREAD, newThread.common_ptr_);

    /* Initialize the state */
    //newThread->state = THREAD_JUST_BORN;
    setCharAt(newThread.common_ptr_ + THREADQUEUE_STATE, THREAD_JUST_BORN);

    /* Add to the alive thread list  */
    //newThread->nextAliveThread = AllThreads;
    setDWordAt(newThread.common_ptr_ + THREADQUEUE_NEXTALIVETHREAD, AllThreads.common_ptr_);
    //AllThreads = newThread;
    setDWordAt(AllThreads.common_ptr_, newThread.common_ptr_);

    return(newThread);
}


static void initInitialThreadBehaviorFromThread(FRAME_HANDLE_FAR exceptionFrameH) {
    INSTANCE_CLASS_FAR thisClass;
    METHOD_FAR thisMethod;
    if (exceptionFrameH.common_ptr_ != 0) {
        /* We have no interest in dealing with exceptions. */
        return;
    }
    thisClass.common_ptr_ = secondStackAsType(INSTANCE_CLASS_FAR);

    thisMethod = getSpecialMethod(thisClass, mainNameAndType);
    if (thisMethod.common_ptr_ == 0) {
        AlertUser(KVM_MSG_CLASS_DOES_NOT_HAVE_MAIN_FUNCTION);
        stopThread();
    } else if ((getWordAt(thisMethod.common_ptr_ + METHOD_ACCESSFLAGS) & ACC_PUBLIC) == 0) {
        AlertUser(KVM_MSG_MAIN_FUNCTION_MUST_BE_PUBLIC);
        stopThread();
    } else {
        ARRAY_FAR af;
        af.common_ptr_ = topStackAsType(int);
        {
        START_TEMPORARY_ROOTS
            DECLARE_TEMPORARY_ROOT(ARRAY_FAR, arguments, af);
            /* Reinitialize the thread for the new method */

            //setSP((MainThread->stack->cells - 1));
            //setSP((getDWordAt(MainThread.common_ptr_ + THREADQUEUE_STACK) + STACKSTRUCT_CELLS - sizeof(far_ptr)));

            //setSP((CurrentThread->stack->cells - 1) + thisMethod->argCount);
            setSP(getDWordAt(CurrentThread.common_ptr_ + THREADQUEUE_STACK) + STACKSTRUCT_CELLS + sizeof(far_ptr) * (-1 + getWordAt(thisMethod.common_ptr_ + METHOD_ARGCOUNT)));

            setFP(0);
            setIP(1/*KILLTHREAD*/);
            pushFrame(thisMethod);
            //((ARRAY *)getLP())[0] = arguments;
            setDWordAt(getLP(), arguments.common_ptr_);
        END_TEMPORARY_ROOTS
        }
        if (getWordAt(thisMethod.common_ptr_ + METHOD_ACCESSFLAGS) & ACC_SYNCHRONIZED) {
            //getFP()->syncObject = (OBJECT)thisClass;
            setDWordAt(getFP() + FRAMESTRUCT_SYNCOBJECT, thisClass.common_ptr_);
            {
                OBJECT_FAR of;
                of.common_ptr_ = thisClass.common_ptr_;
                monitorEnter(of);
            }
        } else {
            //getFP()->syncObject = NULL;
            setDWordAt(getFP() + FRAMESTRUCT_SYNCOBJECT, 0);
        }
    }
}
