/* project : javaonzx
   file    : pool.h
   purpose : constant pool
   author  : valker
*/


#ifndef POOL_H_INCLUDED
#define POOL_H_INCLUDED


/*=========================================================================
* Array types / type indices from JVM Specification (p. 320)
*=======================================================================*/

#define T_BOOLEAN   4
#define T_CHAR      5
#define T_FLOAT     6
#define T_DOUBLE    7
#define T_BYTE      8
#define T_SHORT     9
#define T_INT       10
#define T_LONG      11

/*  Our own extensions to array types */
#define T_VOID      0
#define T_REFERENCE 1
#define T_CLASS     1  /* Another name for T_REFERENCE */


/* Define the range of primitive array types */
#define T_FIRSTPRIMITIVE_TYPE 4
#define T_LASTPRIMITIVETYPE 11


/*=========================================================================
* Field and method access flags (from JVM Specification)
*=======================================================================*/

#define ACC_PUBLIC       0x0001
#define ACC_PRIVATE      0x0002
#define ACC_PROTECTED    0x0004
#define ACC_STATIC       0x0008
#define ACC_FINAL        0x0010
#define ACC_SYNCHRONIZED 0x0020
#define ACC_SUPER        0x0020
#define ACC_VOLATILE     0x0040
#define ACC_TRANSIENT    0x0080
#define ACC_NATIVE       0x0100
#define ACC_INTERFACE    0x0200
#define ACC_ABSTRACT     0x0400
#define ACC_STRICT       0x0800

/* The following flags are not part of the JVM specification.
* We add them when the structure is created. 
*/
#define ACC_ARRAY_CLASS   0x1000  /* Class is an array class */
#define ACC_ROM_CLASS     0x2000  /* ROM class */
/* A ROM class in which neither it nor any */
/* of its superclasses has a <clinit> */
#define ACC_ROM_NON_INIT_CLASS 0x4000  

/* These are the same values as in the JDK.  Makes ROMizer simpler */
#define ACC_DOUBLE        0x4000  /* Field uses two words */
#define ACC_POINTER       0x8000  /* Field is a pointer   */


#endif
