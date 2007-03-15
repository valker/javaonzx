/* project : javaonzx
   file    : locader.c
   purpose : class loader
   author  : valker
*/

#include "jvm_types.h"

BOOL loadedReflectively;


/*=========================================================================
* FUNCTION:      loadClassfile()
* TYPE:          constructor (kind of)
* OVERVIEW:      Load and link the given Java class from a classfile in
*                the system. This function iteratively traverses the
*                superclass hierarchy but recursively traverses the
*                interface hierarchy.
* INTERFACE:
*   parameters:  class structure.  Only the name field is filled in.  All
*                other fields must be NULL and the class status must be
*                CLASS_RAW.
*   returns:     <nothing>
*   throws:      NoClassDefFoundError or ClassNotFoundException,
*                depending on who called this function.
*                ClassCircularityError, ClassFormatError, VerifyError
*                IncompatibleClassChangeError
*=======================================================================*/

void
loadClassfile(INSTANCE_CLASS_FAR InitiatingClass, BOOL fatalErrorIfFail)
{
    /*
    * This must be volatile so that it's value is kept when an exception
    * occurs in the TRY block.
    */
    volatile INSTANCE_CLASS_FAR clazz = InitiatingClass;

    /*
    * The class status must be CLASS_RAW.
    */
    if (clazz->status != CLASS_RAW)
        fatalVMError(KVM_MSG_EXPECTED_CLASS_STATUS_OF_CLASS_RAW);

    TRY {

        /*
        * Iteratively load the raw class bytes of the class and its raw
        * superclasses.
        */
        while (clazz != NULL && clazz->status == CLASS_RAW) {
            clazz->status = CLASS_LOADING;
            loadRawClass(clazz, fatalErrorIfFail);
            if (!fatalErrorIfFail && (clazz->status == CLASS_ERROR)) {
                return;
            }
            clazz->status = CLASS_LOADED;

            /*
            * Now go up to the superclass.
            */
            clazz = clazz->superClass;

            if (clazz != NULL) {
                /*
                * The class will be in CLASS_ERROR if an error occurred
                * during its initialization.
                */
                if (clazz->status == CLASS_ERROR)
                    raiseException(NoClassDefFoundError);
                /*
                * If a class is in CLASS_LOADED state, then this is
                * equivalent to a recursive call to loadClassfile for the
                * class and as such indicates as ClassCircularityError.
                */
                else if (clazz->status == CLASS_LOADED)
                    raiseException(ClassCircularityError);
                /*
                * Any other state is a VM error.
                */
                else if (clazz->status != CLASS_RAW &&
                    clazz->status < CLASS_LINKED)
                    fatalVMError(
                    KVM_MSG_EXPECTED_CLASS_STATUS_OF_CLASS_RAW_OR_CLASS_LINKED);

            }
        }

        /*
        * Iteratively link the class and its unlinked superclass.
        */
        while ((clazz = findSuperMostUnlinked(InitiatingClass)) != NULL) {

#if INCLUDEDEBUGCODE
            if (traceclassloading || traceclassloadingverbose) {
                fprintf(stdout, "Linking class: '%s'\n",
                    getClassName((CLASS)clazz));
            }
#endif

            /*
            * Link the superclass.
            */
            if (clazz->superClass == NULL) {
                /*
                * This must be java.lang.Object.
                */
                if (clazz != JavaLangObject) {
                    raiseExceptionWithMessage(ClassFormatError,
                        KVM_MSG_BAD_SUPERCLASS);
                }
                /*
                * Treat 'java.lang.Object' specially (it has no superclass
                */
                clazz->instSize = 0;
            }
            else {
                INSTANCE_CLASS superClass = clazz->superClass;
                /*
                * Cannot inherit from an array class.
                */
                if (IS_ARRAY_CLASS((CLASS)superClass))
                    raiseExceptionWithMessage(ClassFormatError,
                    KVM_MSG_BAD_SUPERCLASS);

                /*
                * The superclass cannot be an interface. From the
                * JVM Spec section 5.3.5:
                *
                *   If the class of interface named as the direct
                *   superclass of C is in fact an interface, loading
                *   throws an IncompatibleClassChangeError.
                */
                if (superClass->clazz.accessFlags & ACC_INTERFACE) {
                    raiseExceptionWithMessage(IncompatibleClassChangeError,
                        KVM_MSG_CLASS_EXTENDS_INTERFACE);
                }
                /*
                * The superclass cannot be final.
                * Inheriting from a final class is a VerifyError according
                * to J2SE JVM behaviour. There is no explicit
                * documentation in the JVM Spec.
                */
                if (superClass->clazz.accessFlags & ACC_FINAL) {
                    raiseExceptionWithMessage(VerifyError,
                        KVM_MSG_CLASS_EXTENDS_FINAL_CLASS);
                }

                /*
                * The current class must have access to its super class
                */
                verifyClassAccess((CLASS)superClass,clazz);

                /*
                * If this is an interface class, its superclass must be
                * java.lang.Object.
                */
                if (superClass != JavaLangObject &&
                    (clazz->clazz.accessFlags & ACC_INTERFACE)) {
                        raiseExceptionWithMessage(ClassFormatError,
                            KVM_MSG_BAD_SUPERCLASS);
                }

                /*
                * Compute instance size and instance field offset.
                * Make the instance size of the new class "inherit"
                * the instance size of the superclass
                */
                clazz->instSize = superClass->instSize;
            }

            /*
            * Load and link super-interface(s).
            */
            if (clazz->ifaceTable != NULL) {
                unsigned int ifIndex;
                for (ifIndex = 1; ifIndex <= (unsigned int)clazz->ifaceTable[0]; ifIndex++) {
                    INSTANCE_CLASS ifaceClass = (INSTANCE_CLASS)
                        clazz->constPool->entries
                        [clazz->ifaceTable[ifIndex]].clazz;

                    /*
                    * Super-interface cannot be an array class.
                    */
                    if (IS_ARRAY_CLASS((CLASS)ifaceClass)) {
                        raiseExceptionWithMessage(ClassFormatError,
                            KVM_MSG_CLASS_IMPLEMENTS_ARRAY_CLASS);
                    }

                    /*
                    * The class will be in CLASS_ERROR if an error occurred
                    * during its initialization.
                    */
                    if (ifaceClass->status == CLASS_ERROR) {
                        raiseException(NoClassDefFoundError);
                    }
                    /*
                    * If the interface is in CLASS_LOADED state, then this is
                    * a recursive call to loadClassfile for the
                    * interface and as such indicates as ClassCircularityError.
                    */
                    else if (ifaceClass->status == CLASS_LOADED) {
                        raiseException(ClassCircularityError);
                    }
                    else if (ifaceClass->status == CLASS_RAW) {
                        loadClassfile(ifaceClass, TRUE);
                    }
                    else if (ifaceClass->status < CLASS_LINKED) {
                        fatalVMError(KVM_MSG_EXPECTED_CLASS_STATUS_GREATER_THAN_EQUAL_TO_CLASS_LINKED);
                    }

                    /* JVM Spec 5.3.5 */
                    if ((ifaceClass->clazz.accessFlags & ACC_INTERFACE) == 0) {
                        raiseExceptionWithMessage(IncompatibleClassChangeError,
                            KVM_MSG_CLASS_IMPLEMENTS_NON_INTERFACE);
                    }

                    /*
                    * Ensure that the current class has access to the
                    * interface class
                    */
                    verifyClassAccess((CLASS)ifaceClass, clazz);
                }
            }

            FOR_EACH_FIELD(thisField, clazz->fieldTable)
                unsigned short accessFlags = (unsigned short)thisField->accessFlags;

            if ((accessFlags & ACC_STATIC) == 0) {
                thisField->u.offset = clazz->instSize;
                clazz->instSize += (accessFlags & ACC_DOUBLE) ? 2 : 1;
            }
            END_FOR_EACH_FIELD

                /* Move parts of the class to static memory */

                /* **DANGER**
                * While moveClassFieldsToStatic is running, things are in a
                * very strange state as far as GC is concerned.  It is important
                * that this function do no allocation, and that we get rid
                * of the temporary roots registered above before any other
                * allocation is done.
                */
                moveClassFieldsToStatic(clazz);

            clazz->status = CLASS_LINKED;

#if ENABLE_JAVA_DEBUGGER
            if (vmDebugReady) {
                CEModPtr cep = GetCEModifier();
                cep->loc.classID = GET_CLASS_DEBUGGERID(&clazz->clazz);
                cep->threadID = getObjectID((OBJECT)CurrentThread->javaThread);
                cep->eventKind = JDWP_EventKind_CLASS_PREPARE;
                insertDebugEvent(cep);
            }
#endif

#if INCLUDEDEBUGCODE
            if (traceclassloading || traceclassloadingverbose) {
                fprintf(stdout, "Class linked ok\n");
            }
#endif /* INCLUDEDEBUGCODE */
        }

    } CATCH(e) {
        INSTANCE_CLASS c = InitiatingClass;
        /*
        * Prepend the name of the class to the error message (if any)
        * of the exception if this is the initiating class.
        */
        sprintf(str_buffer,"%s",
            getClassName((CLASS)InitiatingClass));
        if (e->message != NULL) {
            int length = strlen(str_buffer);
            char buffer[STRINGBUFFERSIZE];
            getStringContentsSafely(e->message, buffer,
                STRINGBUFFERSIZE - length);
            if (strcmp(getClassName((CLASS)InitiatingClass), buffer)) {
                sprintf(str_buffer + length, ": %s ", buffer);
            }
        }
        e->message = instantiateString(str_buffer,strlen(str_buffer));
        /*
        * Errors occuring during classfile loading are "transient" errors.
        * That is, their cause is temporal in nature and may not occur
        * if a different classfile is submitted at a later point.
        */
        do {
            revertToRawClass(c);
            c = c->superClass;
        } while (c != NULL && c != clazz && c != InitiatingClass);
        THROW(e);
    } END_CATCH
}
