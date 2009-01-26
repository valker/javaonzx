/* project : javaonzx
   file    : thread.h
   purpose : threading
   author  : valker
*/

#ifndef THREAD_H_INCLUDED
#define THREAD_H_INCLUDED

#include "jvm_types.h"
#include "class.h"

/* JAVATHREAD
 *
 * DANGER: This structure is mapped one-to-one with Java thread instances
 *         The fields should be the same as in 'java.lang.Thread'
*/
struct javaThreadStruct {    /* (A true Java object instance) */

    COMMON_OBJECT_INFO(INSTANCE_CLASS_FAR)
    u4       priority;          /* Current priority of the thread */
    THREAD_FAR     VMthread;    /* Backpointer to internal THREAD. */
    INSTANCE_FAR   target;      /* Pointer to the object whose 'run' */
    SHORTARRAY_FAR name;        /* Name of the thread (new in CLDC 1.1) */
                                /* The name is stored as a Java char array */
};

#define JAVATHREADSTRUCT_PRIORITY offsetof(struct javaThreadStruct,priority)
#define JAVATHREADSTRUCT_VMTHREAD offsetof(struct javaThreadStruct,VMthread)
#define JAVATHREADSTRUCT_NAME offsetof(struct javaThreadStruct,name)

/* THREAD */
struct threadQueue {
    THREAD_FAR      nextAliveThread;    /* Queue of threads that are alive; */
    THREAD_FAR      nextThread;         /* Queue of runnable or waiting threads */
    JAVATHREAD_FAR  javaThread;         /* Contains Java-level thread information */
    u4   timeslice;                     /* Calculated from Java-level thread priority */
    STACK_FAR stack;                    /* The execution stack of the thread */

    /* The following four variables are used for storing the */
    /* virtual machine registers of the thread while the thread */
    /* is runnable (= active but not currently given processor time). */
    /* In order to facilitate garbage collection, we store pointers */
    /* to execution stacks as offsets instead of real pointers. */
    u1* ipStore;                        /* Program counter temporary storage (pointer) */
    FRAME_FAR fpStore;                  /* Frame pointer temporary storage (pointer) */
    far_ptr spStore;                    /* Execution stack pointer temp storage (ptr) */
    far_ptr nativeLp;                   /* Used in KNI calls to access local variables */

    MONITOR_FAR monitor_;               /* Monitor whose queue this thread is on */
    u2 monitor_depth;

    THREAD_FAR nextAlarmThread;         /* The next thread on this queue */
    u4   wakeupTime[2];                 /* We can't demand 8-byte alignment of heap
                                           objects  */
    void (*wakeupCall)(THREAD_FAR);     /* Callback when thread's alarm goes off */
    struct {
        i2 depth;
        i4 hashCode;
    } extendedLock;                     /* Used by an object with a FASTLOCK */

    PSTR_FAR pendingException;          /* Name of an exception (class) that the */
    /* thread is about to throw */
    PSTR_FAR exceptionMessage;          /* Message string for the exception */

    enum {
        THREAD_JUST_BORN = 1,     /* Not "start"ed yet */
        THREAD_ACTIVE = 2,        /* Currently running, or on Run queue */
        THREAD_SUSPENDED = 4,     /* Waiting for monitor or alarm */
        THREAD_DEAD = 8,          /* Thread has quit */
        THREAD_MONITOR_WAIT = 16,
        THREAD_CONVAR_WAIT = 32,
        THREAD_DBG_SUSPENDED = 64
    } state;
    BOOL isPendingInterrupt;            /* Don't perform next sleep or wait */

}; /* End of THREAD data structure definition */

/* These are the possible return values from monitorEnter, monitorExit,
* monitorWait, and monitorNotify.
*/
enum MonitorStatusType {
    MonitorStatusOwn,       /* The process owns the monitor */
    MonitorStatusRelease,   /* The process has released the monitor */
    MonitorStatusWaiting,   /* The process is waiting for monitor/condvar */
    MonitorStatusError      /* An error has occurred */
};

#define THREADQUEUE_NEXTTHREAD offsetof(struct threadQueue,nextThread)
#define THREADQUEUE_NEXTALIVETHREAD offsetof(struct threadQueue,nextAliveThread)
#define THREADQUEUE_JAVATHREAD offsetof(struct threadQueue,javaThread)
#define THREADQUEUE_STATE offsetof(struct threadQueue,state)
#define THREADQUEUE_STACK offsetof(struct threadQueue,stack)
#define THREADQUEUE_TIMESLICE offsetof(struct threadQueue,timeslice)

#define SIZEOF_THREAD sizeof(struct threadQueue)


extern THREAD_FAR AllThreads;       /* List of all threads */
extern THREAD_FAR RunnableThreads;  /* Queue of all threads that can be run */
extern THREAD_FAR CurrentThread;    /* Current thread */
extern THREAD_FAR MainThread;       /* For debugger code to access */

/* Threads waiting for a timer interrupt */
extern THREAD_FAR TimerQueue;

extern MONITOR_FAR MonitorCache;
extern u2 AliveThreadCount;         /* Number of alive threads */
extern u2 Timeslice;                /* Time slice counter for multitasking */

void   InitializeThreading(INSTANCE_CLASS_FAR, ARRAY_FAR);
void   stopThread(void);
enum MonitorStatusType  monitorEnter(OBJECT_FAR object);
enum MonitorStatusType  monitorExit(OBJECT_FAR object, PSTR_FAR exceptionName);
enum MonitorStatusType  monitorWait(OBJECT_FAR object, long64 time);


#endif
