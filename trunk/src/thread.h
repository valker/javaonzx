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
#define JAVATHREADSTRUCT_NAME offsetof(struct javaThreadStruct,name)


extern THREAD_FAR AllThreads;       /* List of all threads */
extern THREAD_FAR RunnableThreads;  /* Queue of all threads that can be run */
extern THREAD_FAR CurrentThread;    /* Current thread */
extern THREAD_FAR MainThread;       /* For debugger code to access */

/* Threads waiting for a timer interrupt */
extern THREAD_FAR TimerQueue;

extern MONITOR_FAR MonitorCache;

void   InitializeThreading(INSTANCE_CLASS_FAR, ARRAY_FAR);


#endif
