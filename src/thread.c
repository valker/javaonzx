/* project : javaonzx
   file    : thread.c
   purpose : threading
   author  : valker
*/

#include <stddef.h>
#include "jvm_types.h"
#include "thread.h"
#include "garbage.h"
#include "root_code.h"

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
initInitialThreadBehaviorFromThread(FRAME_HANDLE);

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
            PSTR_FAR name = {address_24_of("Thread-0")};
            setDWordAt(javaThread.common_ptr_ + JAVATHREADSTRUCT_NAME, createCharArray(name, 8, &unused, FALSE).common_ptr_);
        }

        MainThread = BuildThread(&javaThread);

        /* AllThreads is initialized to NULL by the garbage collector.
         *
         * Ensure that the thread list is properly initialized
         * and set mainThread as the active (CurrentThread) thread
         */
        MainThread->nextThread = NIL;
        AliveThreadCount = 1;
        Timeslice = BASETIMESLICE;
        MainThread->state = THREAD_ACTIVE;

        /* Initialize VM registers */
        CurrentThread = MainThread;
        RunnableThreads = NULL;
        TimerQueue = NULL;

        setSP((MainThread->stack->cells - 1));
        setFP(NULL);
        setIP(KILLTHREAD);

        /* We can't create a frame consisting of "method", since its class
         * has not yet been initialized, (and this would mess up the
         * garbage collector).  So we set up a pseudo-frame, and arrange
         * for the interpreter to do the work for us.
         */
        pushFrame(RunCustomCodeMethod);
        pushStackAsType(CustomCodeCallbackFunction,
                        initInitialThreadBehaviorFromThread);
        /* We want to push method, but that would confuse the GC, since
         * method is a pointer into the middle of a heap object.  Only
         * heap objects, and things that are clearly not on the heap, can
         * be pushed onto the stack.
         */
        pushStackAsType(INSTANCE_CLASS, mainClass);
        pushStackAsType(ARRAY, arguments);
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
    THREAD newThread;
    JAVATHREAD javaThread;
    START_TEMPORARY_ROOTS
        DECLARE_TEMPORARY_ROOT(THREAD, newThreadX,
                               (THREAD)callocObject(SIZEOF_THREAD, GCT_THREAD));
        STACK newStack = (STACK)callocObject(sizeof(struct stackStruct)
                                                     / CELL, GCT_EXECSTACK);
        /* newStack->next = NULL;  Already zero from calloc */
        newStack->size = STACKCHUNKSIZE;
        newThreadX->stack = newStack;

#if INCLUDEDEBUGCODE
        if (tracethreading) {
            TraceThread(newThreadX, "Created");
        }
        if (tracestackchunks) {
            printf("Created a new stack (thread: %lx, first chunk: %lx, chunk size: %ld\n",
                (long)newThreadX, (long)newStack, (long)STACKCHUNKSIZE);
        }
#endif /* INCLUDEDEBUGCODE */

        newThread = newThreadX;
    END_TEMPORARY_ROOTS

    /* Time slice will be initialized to default value */
    newThread->timeslice = BASETIMESLICE;

    /* Link the THREAD to the JAVATHREAD */
    javaThread = unhand(javaThreadH);
    newThread->javaThread = javaThread;
    javaThread->VMthread = newThread;

    /* Initialize the state */
    newThread->state = THREAD_JUST_BORN;
#if ENABLE_JAVA_DEBUGGER
    newThread->nextOpcode = NOP;
#endif

    /* Add to the alive thread list  */
    newThread->nextAliveThread = AllThreads;
    AllThreads = newThread;

    return(newThread);
}
