#include "Python.h"
#include <unistd.h>
#include <ctype.h>

#include "globus_common.h"
#include "globus_ftp_client.h"
#include "globus_ftp_client_debug_plugin.h"
#include "globus_ftp_client_restart_marker_plugin.h"
#include "globus_ftp_client_perf_plugin.h"
#include "globus_ftp_client_throughput_plugin.h"

#include "globus_ftp_control.h"


// Some notes about threads
// 
// There are basically two types of C functions here. The first
// type are functions that are directly called from Python, that
// is from the main thread servicing the Python interpreter. For
// example the function call to initiate a third party transfer.
// 
// These functions will always have the Python Global Interpreter
// Lock (GIL) because whenenver Python calls a function in an
// external module or extension it gives the function the GIL.
// 
// Because there is a call inside of most of these functions out
// to a Globus library, and because most of those calls involve 
// making a network connection, the calls can take some time.
// This could result in the function holding the GIL for a long
// time and not allowing the rest of any threads that need to work
// with Python to make progress.
// 
// So these functions instead give up the GIL right before calling
// out to the Globus library and then acquire it again when the
// Globus call returns. We accomplish this using two macros 
// from Python:
// 
//     Py_BEGIN_ALLOW_THREADS
// 
//         make an external Globus call here
// 
//     Py_END_ALLOW_THREADS
// 
// The second type of C function are functions called as callbacks.
// These are called by threads that are not the main thread, ie. 
// threads created by the Globus libraries. These functions need
// to interact with the Python interpreter, usually by calling
// a Python callback function with arguments passed from the Globus
// library.
// 
// These callback functions being called from a Globus thread cannot
// do any Python operations at all until and unless they have the GIL,
// and the state of the thread is saved in a Python global variable
// (see the Python C API for details). When done interacting with
// the Python interpreter these functions must set the state of the 
// thread back to what is was by calling a particular Python C function
// and then release the GIL.
// 
// We accomplish this using this Python C API idiom:
// 
//     PyGILState_STATE gstate;
//     gstate = PyGILState_Ensure();
// 
//         Perform Python actions here
// 
//     PyGILState_Release(gstate);
// 
// Lastly, because we are using multiple threads (via Globus) and the
// threads are not created by the Python interpreter we need to make
// sure Python is "ready" for working with the threads. This is not
// done automatically by Python since it would not be efficient for
// pure Python code.
// 
// To initialize Python and get the main thread ready to handle
// the Globus threads we call during the module initialization the 
// function
// 
//     PyEval_InitThreads();
// 
// We do this before activating the Globus modules so we are sure
// there is only the main thread and hence things will be
// properly initialized.


// the following is useful if it is necessary to track down
// reference counting problems
//
/* BEGIN From Python Cookbook XXX */
#if defined(Py_DEBUG) || defined(DEBUG)
extern void _Py_CountReferences(FILE*);
#define CURIOUS(x) { fprintf(stderr, __FILE__ ":%d ", __LINE__); x;}
#else
#define CURIOUS(x) fprintf(stderr, "No DEBUG!\n");
#endif

#define MARKER()        CURIOUS(fprintf(stderr, "\n"))
#define DESCRIBE(x)     CURIOUS(fprintf(stderr, " " #x "=%d\n", x))
#define DESCRIBE_HEX(x) CURIOUS(fprintf(stderr, " " #x "=%08x\n", x))
#define COUNTREFS()     CURIOUS(_Py_CountReferences(stderr))
/* END From Python Cookbook XXX */

// Globus modules that need to be initialized 
static globus_module_descriptor_t   *modules[] = {
  GLOBUS_COMMON_MODULE,
  GLOBUS_IO_MODULE,
  GLOBUS_FTP_CLIENT_MODULE,
};                        

static void prepare()
{}

static void parent()
{}

static void child()
{ 
  sigset_t sm;
  sigemptyset(&sm);
  pthread_sigmask(SIG_SETMASK, &sm, NULL);
}


#define NMODS   (sizeof(modules) / sizeof(globus_module_descriptor_t *))

//
// This section of the code is for data structures. Please
// put any data structures needed in this section.
//
//
//
//
//

// used to store pointers to the Python objects that should
// be used during a callback for completion of third party transfer
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} third_party_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a checksum operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
    char cksm[32];         // the checksum value

} cksm_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a mkdir operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} mkdir_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a rmdir operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} rmdir_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a delete operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} delete_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a move operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} move_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for a chmod operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} chmod_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for completion of a get operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} get_complete_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for completion of a verbose list operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} verbose_list_complete_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a data callback for a get or put operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
    PyObject * pybuffer;   // Python object for the Python buffer 
} get_data_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a performance marker callback 
typedef struct
{
    PyObject * begincb;     // Python object for the Python function to call at beginning of transfer
    PyObject * markercb;    // Python object for the Python function to call when perf marker is received
    PyObject * completecb;  // Python object for the Python function to call at completion of a transfer
    PyObject * userarg;     // Python object for the user arg passed in and then passed to the callback Python functions
} perf_plugin_callback_bucket_t;

// used to store pointers to the Python objects that should
// be used during a callback for an exists operation
typedef struct
{
    PyObject * pyfunction; // Python object for the Python function to call as callback
    PyObject * pyarg;      // Python object for the Python argument to pass in to the callback
} exists_callback_bucket_t;


//
// This section of the code is for auxiliary functions
// that are not called directly by the Python module,
// such as functions to pass through callbacks from the C code
// to the Python code.
//

// callback for the completion of third party transfers
static void third_party_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;

    third_party_callback_bucket_t  * callbackBucket;

    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    callbackBucket = (third_party_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object

    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);


    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of checksum operations
static void cksm_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    cksm_callback_bucket_t * callbackBucket = (cksm_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(sOOO)", callbackBucket -> cksm, arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of mkdir operation
static void mkdir_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    mkdir_callback_bucket_t * callbackBucket = (mkdir_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of rmdir operation
static void rmdir_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    rmdir_callback_bucket_t * callbackBucket = (rmdir_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of delete operation
static void delete_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    delete_callback_bucket_t * callbackBucket = (delete_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of move operation
static void move_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    move_callback_bucket_t * callbackBucket = (move_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of chmod operation
static void chmod_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    chmod_callback_bucket_t * callbackBucket = (chmod_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the completion of get operations
static void get_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    get_complete_callback_bucket_t * callbackBucket = (get_complete_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for the data read of get operations
static void get_data_callback(
        void * user_data, 
        globus_ftp_client_handle_t * handle, 
        globus_object_t * error,
        globus_byte_t * buffer,
        globus_size_t length,
        globus_off_t offset,
        globus_bool_t eof) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;
    PyObject * bufferObj;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    get_data_callback_bucket_t * callbackBucket = (get_data_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    // as well as the buffer object
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;
    bufferObj= PyBuffer_FromReadWriteMemory((void*) buffer, length * sizeof(globus_byte_t));


    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOOOlli)", arg, handleObj, errorObject, bufferObj, (long) length, (long) offset, (int) eof);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(bufferObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}

// callback for performance marker plugin that is called
// when a transfer starts
static void perf_plugin_begin_cb(
    void * user_specific,
    globus_ftp_client_handle_t * handle,
    const char * source_url,
    const char * dest_url,
    globus_bool_t restart)
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;

    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    perf_plugin_callback_bucket_t * callbackBucket = (perf_plugin_callback_bucket_t *) user_specific;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> begincb;
    arg = callbackBucket -> userarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOssi)", arg, handleObj, source_url, dest_url, (int) restart);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    return;

}

// callback for performance marker plugin that is called
// each time a performance marker is received
static void perf_plugin_marker_cb(
    void * user_specific,
    globus_ftp_client_handle_t * handle,
    long time_stamp_int,
    char time_stamp_tenth,
    int stripe_ndx,
    int num_stripes,
    globus_off_t nbytes)
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;

    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    perf_plugin_callback_bucket_t * callbackBucket = (perf_plugin_callback_bucket_t *) user_specific;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> markercb;
    arg = callbackBucket -> userarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOlbiil)", 
        arg, handleObj, time_stamp_int, time_stamp_tenth, 
        stripe_ndx, num_stripes, (long) nbytes);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    return;

}

// callback for performance marker plugin that is called
// when a transfer completes
static void perf_plugin_complete_cb(
    void * user_specific,
    globus_ftp_client_handle_t * handle,
    globus_bool_t success)
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;

    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    perf_plugin_callback_bucket_t * callbackBucket = (perf_plugin_callback_bucket_t *) user_specific;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> completecb;
    arg = callbackBucket -> userarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOi)", arg, handleObj, (int) success );

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    return;
}

// callback for the completion of exists operation
static void exists_complete_callback(void * user_data, globus_ftp_client_handle_t * handle, globus_object_t * error) 
{
    PyObject * func;
    PyObject * arglist; 
    PyObject * result; 
    PyObject * arg;
    PyObject * handleObj;
    PyObject * errorObject;


    // cast the user_data that the GridFTP libraries are passing in to the
    // callback structure where we previously stored the Python function and
    // arguments to call
    exists_callback_bucket_t * callbackBucket = (exists_callback_bucket_t *) user_data;

    // we need to obtain the Python GIL before this thread can manipulate any Python object
    PyGILState_STATE gstate;
    gstate = PyGILState_Ensure();

    // pick off the function and argument pointers we want to pass back into Python
    func = callbackBucket -> pyfunction;
    arg = callbackBucket -> pyarg;

    // create a handle object to pass back into Python
    handleObj = PyCObject_FromVoidPtr((void *) handle, NULL);

    // create an error object to pass back into Python
    if (error){
        errorObject = Py_BuildValue("s", globus_error_print_chain(error));
    } else{
        errorObject = Py_BuildValue("s", NULL);
    }

    // prepare the arg list to pass into the Python callback function
    arglist = Py_BuildValue("(OOO)", arg, handleObj, errorObject);

    // now call the Python callback function
    result = PyEval_CallObject(func, arglist);

    if (result == NULL) {

        // something went wrong so print to stderr
        PyErr_Print();
    }

    // take care of reference handling
    Py_DECREF(handleObj);
    Py_DECREF(arglist);
    Py_XDECREF(result);
    Py_XDECREF(errorObject);

    // release the Python GIL from this thread
    PyGILState_Release(gstate);

    // free the space the callback bucket was holding
    free(callbackBucket);

    return;
}



//
// This section of the code is for functions called 
// by the Python module.
//
//
//
//
//


// activate the Globus modules
PyObject * gridftp_modules_activate(PyObject * self, PyObject * args)
{
    int           i;
    int           rc;

    // this is new with Globus 5.2.x
    Py_BEGIN_ALLOW_THREADS
	globus_thread_set_model("pthread");
    Py_END_ALLOW_THREADS
	


    for (i = 0; i < NMODS; i++){
    
        Py_BEGIN_ALLOW_THREADS

        rc = globus_module_activate(modules[i]);

        Py_END_ALLOW_THREADS

        if (rc != GLOBUS_SUCCESS) {
	  PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to activate Globus module");
        }
    }

    // one of the globus modules changes the signal handling behavior.
    // http://jira.globus.org/browse/GT-360
    // The next line ensures that any forked subprocesses still catch
    // SIGTERM, SIGHUP, SIGINT. Without this line, these signals are
    // not passed along and forked processes will miss these messages.
    Py_BEGIN_ALLOW_THREADS
      pthread_atfork(&prepare, &parent, &child);
    Py_END_ALLOW_THREADS


    Py_RETURN_NONE;
}

// deactivate the Globus modules
PyObject * gridftp_modules_deactivate(PyObject * self, PyObject * args)
{
    int   i;

    for (i = NMODS - 1; i >= 0; i--){

        Py_BEGIN_ALLOW_THREADS

        globus_module_deactivate(modules[i]);

        Py_END_ALLOW_THREADS

    }
    
    Py_RETURN_NONE;
}

// create a buffer for storing data from a get or put operation
// and return a wrapped pointer to the buffer
PyObject * gridftp_create_buffer(PyObject * self, PyObject * args)
{

    globus_byte_t * buffer = NULL;
    unsigned long size;
    char msg[2048] = "";

    PyObject * bufferObj;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "k", &size)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

    // allocate the memory
    Py_BEGIN_ALLOW_THREADS

    buffer = globus_malloc(sizeof(globus_byte_t) * size);

    Py_END_ALLOW_THREADS

    if (buffer == NULL) {
        sprintf(msg, "gridftpwrapper: unable to create buffer");
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // set the memory contents to zero
    memset(buffer, 0, (size_t) size);

    // wrap pointer to buffer and return
    bufferObj = PyCObject_FromVoidPtr((void *) buffer, NULL);

    return bufferObj;
}

// destroy a buffer previously created
PyObject * gridftp_destroy_buffer(PyObject * self, PyObject * args)
{

    globus_byte_t * buffer = NULL;
    PyObject * bufferObj;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &bufferObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

    buffer = (globus_byte_t *) PyCObject_AsVoidPtr(bufferObj);

    // free the memory
    Py_BEGIN_ALLOW_THREADS

    globus_libc_free(buffer);

    Py_END_ALLOW_THREADS

    // return None to indicate success
    Py_RETURN_NONE;
}

// return a Python string representation of the data in a buffer
PyObject * gridftp_buffer_to_string(PyObject * self, PyObject * args)
{

    globus_byte_t * buffer = NULL;
    unsigned long size;
    PyObject * bufferObj;
    PyObject * pyString;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Ok", &bufferObj, &size)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

    buffer = (globus_byte_t *) PyCObject_AsVoidPtr(bufferObj);

    // free the memory
    Py_BEGIN_ALLOW_THREADS

    pyString = PyString_FromStringAndSize((char*) buffer, size);

    Py_END_ALLOW_THREADS

    return pyString; 
}

// initialize a handle attribute and return a wrapped pointer
// to it
PyObject * gridftp_handleattr_init(PyObject *self, PyObject *args)
{
    globus_ftp_client_handleattr_t  * handle_attr = NULL;
    globus_result_t gridftp_result;
    PyObject * handleAttr = NULL;
    char msg[2048] = "";

    handle_attr = (globus_ftp_client_handleattr_t *) globus_malloc(sizeof(globus_ftp_client_handleattr_t));

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handleattr_init(handle_attr);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to initialize handle attribute", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    

    // wrap pointer to handle attribute and return
    handleAttr = PyCObject_FromVoidPtr((void *) handle_attr, NULL);

    return handleAttr;
}

// initialize a handle and return a wrapper pointer to it
PyObject * gridftp_handle_init(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handle = NULL;
    globus_ftp_client_handleattr_t * handle_attr = NULL;
    PyObject * handleAttr = NULL;
    PyObject * handleObject = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = "";

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &handleAttr)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse handle attr object");
        return NULL;
    }
 
    if PyCObject_Check(handleAttr){
        handle_attr = (globus_ftp_client_handleattr_t *) PyCObject_AsVoidPtr(handleAttr);
    } else {
        sprintf(msg, "gridftpwrapper: unable to obtain pointer to handle_attr");
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    handle = (globus_ftp_client_handle_t *) globus_malloc(sizeof(globus_ftp_client_handle_t));

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handle_init(handle, handle_attr);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to initialize handle", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    

    // wrap pointer to handle and return
    handleObject = PyCObject_FromVoidPtr((void *) handle, NULL);

    return handleObject;

}

// destroy a previously created handle attribute
PyObject * gridftp_handleattr_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_client_handleattr_t * handle_attr = NULL;
    PyObject * handleAttr = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &handleAttr)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse handle attr object");
        return NULL;
    }
 
    handle_attr = (globus_ftp_client_handleattr_t *) PyCObject_AsVoidPtr(handleAttr);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handleattr_destroy(handle_attr);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to destroy handle attr", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// set the 'cache all' setting on a handle attribute
PyObject * gridftp_handleattr_set_cache_all(PyObject *self, PyObject *args)
{
    globus_ftp_client_handleattr_t * handle_attr = NULL;
    PyObject * handleAttr = NULL;
    int cache_all;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &handleAttr, &cache_all)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to arguments");
        return NULL;
    }
 
    handle_attr = (globus_ftp_client_handleattr_t *) PyCObject_AsVoidPtr(handleAttr);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handleattr_set_cache_all(handle_attr, (globus_bool_t) cache_all);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to set cache all boolean", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// destroy a previously created handle
PyObject * gridftp_handle_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handle = NULL;
    PyObject * handleObject = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &handleObject)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse handle object");
        return NULL;
    }
 
    handle = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObject);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handle_destroy(handle);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to destroy handle", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// initialize an operation attribute and return a wrapped pointer
// to it
PyObject * gridftp_operationattr_init(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t  * operation_attr = NULL;
    globus_result_t gridftp_result;
    PyObject * opAttr = NULL;
    char msg[2048] = "";

    operation_attr = (globus_ftp_client_operationattr_t *) globus_malloc(sizeof(globus_ftp_client_operationattr_t));

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_operationattr_init(operation_attr);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to initialize operation attribute", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // wrap pointer to operation attribute and return
    opAttr = PyCObject_FromVoidPtr((void *) operation_attr, NULL);

    return opAttr;
}

// destroy a previously created operation attribute
PyObject * gridftp_operationattr_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t * operation_attr = NULL;
    PyObject * opAttr = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &opAttr)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse operation attr object");
        return NULL;
    }
 
    operation_attr = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttr);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_operationattr_destroy(operation_attr);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to destroy operation attr", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// set the mode on an operation attribute
PyObject * gridftp_operationattr_set_mode(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t * operation_attr = NULL;
    PyObject * opAttr = NULL;
    int mode = -1;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &opAttr, &mode)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    operation_attr = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttr);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_operationattr_set_mode(operation_attr, (globus_ftp_control_mode_t) mode);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to set mode", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// set the parallelism for an operation attribute
PyObject * gridftp_operationattr_set_parallelism(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t * operation_attr = NULL;
    globus_ftp_control_parallelism_t * parallelism = NULL;
    PyObject * opAttr = NULL;
    PyObject * parallelObj = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OO", &opAttr, &parallelObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    operation_attr = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttr);
    parallelism = (globus_ftp_control_parallelism_t *) PyCObject_AsVoidPtr(parallelObj);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_operationattr_set_parallelism(operation_attr, parallelism);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to set parallelism", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}



// FIXME
PyObject * gridftp_operationattr_set_disk_stack(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t * operation_attr = NULL;
    PyObject * opAttr = NULL;
    char * driver_list="";
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Os", &opAttr, &driver_list)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    operation_attr = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttr);

    Py_BEGIN_ALLOW_THREADS
    gridftp_result = globus_ftp_client_operationattr_set_disk_stack(operation_attr, driver_list);
    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to set_disk_stack", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;
}


// set the tcp buffer for an operation attribute
PyObject * gridftp_operationattr_set_tcp_buffer(PyObject *self, PyObject *args)
{
    globus_ftp_client_operationattr_t * operation_attr = NULL;
    globus_ftp_control_tcpbuffer_t * tcpbuffer = NULL;
    PyObject * opAttr = NULL;
    PyObject * tcpbufferObj = NULL;
    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OO", &opAttr, &tcpbufferObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    operation_attr = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttr);
    tcpbuffer = (globus_ftp_control_tcpbuffer_t *) PyCObject_AsVoidPtr(tcpbufferObj);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_operationattr_set_tcp_buffer(operation_attr, tcpbuffer);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to set tcpbuffer", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// initialize a parallelism type and return a wrapper pointer to it
PyObject * gridftp_parallelism_init(PyObject *self, PyObject *args)
{
    globus_ftp_control_parallelism_t  * parallelism = NULL;
    PyObject * parallelismObj = NULL;
    char msg[2048] = "";

    parallelism = (globus_ftp_control_parallelism_t *) globus_malloc(sizeof(globus_ftp_control_parallelism_t));

    if (parallelism == NULL){
        sprintf(msg, "gridftpwrapper: unable to initialize parallelism");
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // wrap pointer to parallelism_t and return
    parallelismObj = PyCObject_FromVoidPtr((void *) parallelism, NULL);

    return parallelismObj;
}

// destroy a previously created parallelism type
PyObject * gridftp_parallelism_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_control_parallelism_t * parallelism = NULL;
    PyObject * parallelismObj = NULL;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &parallelismObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    parallelism = (globus_ftp_control_parallelism_t *) PyCObject_AsVoidPtr(parallelismObj);

    free(parallelism);

    // return None to indicate success
    Py_RETURN_NONE;

}

// set the mode on a parallelism type
PyObject * gridftp_parallelism_set_mode(PyObject *self, PyObject *args)
{
    globus_ftp_control_parallelism_t * parallelism = NULL;
    PyObject * parallelismObj = NULL;
    int mode = -1;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &parallelismObj, &mode)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    parallelism = (globus_ftp_control_parallelism_t *) PyCObject_AsVoidPtr(parallelismObj);
    parallelism -> mode = mode;

    // return None to indicate success
    Py_RETURN_NONE;

}

// set the size on a parallelism type
PyObject * gridftp_parallelism_set_size(PyObject *self, PyObject *args)
{
    globus_ftp_control_parallelism_t * parallelism = NULL;
    PyObject * parallelismObj = NULL;
    int size = -1;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &parallelismObj, &size)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    parallelism = (globus_ftp_control_parallelism_t *) PyCObject_AsVoidPtr(parallelismObj);
    parallelism -> fixed.size = size;

    // return None to indicate success
    Py_RETURN_NONE;

}

// initialize a tcpbuffer type and return a wrapped pointer to it
PyObject * gridftp_tcpbuffer_init(PyObject *self, PyObject *args)
{
    globus_ftp_control_tcpbuffer_t  * tcpbuffer = NULL;
    PyObject * tcpbufferObj = NULL;
    char msg[2048] = "";

    tcpbuffer = (globus_ftp_control_tcpbuffer_t *) globus_malloc(sizeof(globus_ftp_control_tcpbuffer_t));

    if (tcpbuffer == NULL){
        sprintf(msg, "gridftpwrapper: unable to initialize tcpbuffer");
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // wrap pointer to tcpbuffer_t and return
    tcpbufferObj = PyCObject_FromVoidPtr((void *) tcpbuffer, NULL);

    return tcpbufferObj;
}

// destroy a previously created tcpbuffer type
PyObject * gridftp_tcpbuffer_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_control_tcpbuffer_t * tcpbuffer = NULL;
    PyObject * tcpbufferObj = NULL;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &tcpbufferObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    tcpbuffer = (globus_ftp_control_tcpbuffer_t *) PyCObject_AsVoidPtr(tcpbufferObj);

    free(tcpbuffer);

    // return None to indicate success
    Py_RETURN_NONE;

}

// set the mode for a tcpbuffer type
PyObject * gridftp_tcpbuffer_set_mode(PyObject *self, PyObject *args)
{
    globus_ftp_control_tcpbuffer_t * tcpbuffer = NULL;
    PyObject * tcpbufferObj = NULL;
    int mode = -1;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &tcpbufferObj, &mode)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    tcpbuffer = (globus_ftp_control_tcpbuffer_t *) PyCObject_AsVoidPtr(tcpbufferObj);
    tcpbuffer -> mode = mode;

    // return None to indicate success
    Py_RETURN_NONE;

}

// set the size for a tcpbuffer type
PyObject * gridftp_tcpbuffer_set_size(PyObject *self, PyObject *args)
{
    globus_ftp_control_tcpbuffer_t * tcpbuffer = NULL;
    PyObject * tcpbufferObj = NULL;
    int size = -1;

    // get Python arguments
    if (!PyArg_ParseTuple(args, "Oi", &tcpbufferObj, &size)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    tcpbuffer = (globus_ftp_control_tcpbuffer_t *) PyCObject_AsVoidPtr(tcpbufferObj);
    tcpbuffer -> fixed.size = size;

    // return None to indicate success
    Py_RETURN_NONE;

}

// initialize a third party transfer
PyObject * gridftp_third_party_transfer(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * src = NULL;
    globus_ftp_client_operationattr_t * src_operation_attrp = NULL;
    char * dst = NULL;
    globus_ftp_client_operationattr_t * dst_operation_attrp = NULL;
    // globus_ftp_client_restart_marker_t * restart_markerp = NULL;

    PyObject * handleObj;
    PyObject * srcOpAttrObj;
    PyObject * dstOpAttrObj;
    PyObject * restartMarkerObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    third_party_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOsOOOO", 
            &handleObj, 
            &src, 
            &srcOpAttrObj,
            &dst,
            &dstOpAttrObj,
            &restartMarkerObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    src_operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(srcOpAttrObj);
    dst_operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(dstOpAttrObj);
    //restart_markerp = (globus_ftp_client_restart_marker_t *) PyCObject_AsVoidPtr(restartMarkerObj);

    // create a third party callback struct to hold the callback information
    callbackBucket = (third_party_callback_bucket_t *) globus_malloc(sizeof(third_party_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the third party transfer 

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_third_party_transfer(
                        handlep,
                        src,
                        src_operation_attrp,
                        dst,
                        dst_operation_attrp,
                        // restart_markerp,
                        NULL,
                        third_party_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start third party transfer", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    
    // return None to indicate success
    Py_RETURN_NONE;

}

// compute the md5 checksum 
// note that it is returned in a callback
PyObject * gridftp_cksm(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;
    int offset = -1;
    int length = -1;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    cksm_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOiiOO", 
            &handleObj, 
            &url, 
            &OpAttrObj,
            &offset,
            &length,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a cksm callback struct to hold the callback information
    callbackBucket = (cksm_callback_bucket_t *) globus_malloc(sizeof(cksm_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the checksum operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_cksm(
                        handlep,
                        url,
                        operation_attrp,
                        callbackBucket -> cksm,
                        (globus_off_t) offset,
                        (globus_off_t ) length,
                        "MD5",
                        cksm_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start checksum operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// make a directory
// the status is returned in a callback
PyObject * gridftp_mkdir(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    mkdir_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOO", 
            &handleObj, 
            &url, 
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a mkdir callback struct to hold the callback information
    callbackBucket = (mkdir_callback_bucket_t *) globus_malloc(sizeof(mkdir_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the checksum operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_mkdir(
                        handlep,
                        url,
                        operation_attrp,
                        mkdir_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start mkdir operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// remove a directory
// the status is returned in a callback
PyObject * gridftp_rmdir(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    rmdir_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOO", 
            &handleObj, 
            &url, 
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a rmdir callback struct to hold the callback information
    callbackBucket = (rmdir_callback_bucket_t *) globus_malloc(sizeof(rmdir_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the checksum operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_rmdir(
                        handlep,
                        url,
                        operation_attrp,
                        rmdir_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start rmdir operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// delete a file
// the status is returned in a callback
PyObject * gridftp_delete(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    delete_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOO", 
            &handleObj, 
            &url, 
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a delete callback struct to hold the callback information
    callbackBucket = (delete_callback_bucket_t *) globus_malloc(sizeof(delete_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the checksum operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_delete(
                        handlep,
                        url,
                        operation_attrp,
                        delete_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start delete operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// move (that is, rename) a file
// the status is returned in a callback
PyObject * gridftp_move(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * src = NULL;
    char * dst = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    move_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OssOOO", 
            &handleObj, 
            &src, 
            &dst,
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a move callback struct to hold the callback information
    callbackBucket = (move_callback_bucket_t *) globus_malloc(sizeof(move_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the checksum operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_move(
                        handlep,
                        src,
                        dst,
                        operation_attrp,
                        move_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start move operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// chmod a file
// the status is returned in a callback
PyObject * gridftp_chmod(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    int mode = 0;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    chmod_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsiOOO", 
            &handleObj, 
            &url, 
            &mode,
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create a chmod callback struct to hold the callback information
    callbackBucket = (chmod_callback_bucket_t *) globus_malloc(sizeof(chmod_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the chmod operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_chmod(
                        handlep,
                        url,
                        mode,
                        operation_attrp,
                        chmod_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start chmod operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// start a gridftp get operation
PyObject * gridftp_get(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * src = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;
    // globus_ftp_client_restart_marker_t * restart_markerp = NULL;

    PyObject * handleObj;
    PyObject * opAttrObj;
    PyObject * restartMarkerObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    get_complete_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOOO", 
            &handleObj, 
            &src, 
            &opAttrObj,
            &restartMarkerObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttrObj);
    //restart_markerp = (globus_ftp_client_restart_marker_t *) PyCObject_AsVoidPtr(restartMarkerObj);

    // create a get callback struct to hold the callback information
    callbackBucket = (get_complete_callback_bucket_t *) globus_malloc(sizeof(get_complete_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the get transfer 

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_get(
                        handlep,
                        src,
                        operation_attrp,
                        NULL,
                        get_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start get transfer", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// start a gridftp verbose list operation
PyObject * gridftp_verbose_list(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * opAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    verbose_list_complete_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOO", 
            &handleObj, 
            &url, 
            &opAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(opAttrObj);

    // create a verbose list callback struct to hold the callback information
    callbackBucket = (verbose_list_complete_callback_bucket_t *) globus_malloc(sizeof(verbose_list_complete_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the verbose list operation 

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_verbose_list(
                        handlep,
                        url,
                        operation_attrp,
                        get_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start verbose list operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// register the data callback function for a get operation
PyObject * gridftp_register_read(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    globus_byte_t * buffer = NULL;
    unsigned long buffer_length = 0;

    PyObject * handleObj;
    PyObject * bufferObj;
    PyObject * dataCallbackFunctionObj;
    PyObject * dataCallbackArgObj;

    get_data_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OOkOO", 
            &handleObj, 
            &bufferObj, 
            &buffer_length,
            &dataCallbackFunctionObj,
            &dataCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    buffer = (globus_byte_t *) PyCObject_AsVoidPtr(bufferObj);

    // create a data callback struct to hold the callback information
    callbackBucket = (get_data_callback_bucket_t *) globus_malloc(sizeof(get_data_callback_bucket_t));
    callbackBucket -> pyfunction = dataCallbackFunctionObj;
    callbackBucket -> pyarg = dataCallbackArgObj;
    callbackBucket -> pybuffer = bufferObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);
    Py_XINCREF(callbackBucket -> pybuffer);

    // register the read

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_register_read(
                        handlep,
                        buffer,
                        (globus_size_t) buffer_length,
                        get_data_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to register read", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// abort whatever operation is currently going on for a handle
PyObject * gridftp_abort(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;

    PyObject * handleObj;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "O", &handleObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);

    // abort the current operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_abort(handlep);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to abort", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// initialize a performance marker plugin and return a wrapped
// pointer to it and a wrapped pointer to the callback struct
// that is used to carry around the pointers to the Python 
// callback functions that should be called when a transfer
// starts, when a performance marker is received, and when the
// transfer completes
PyObject * gridftp_perf_plugin_init(PyObject *self, PyObject *args)
{
    globus_ftp_client_plugin_t * pluginp = NULL;
    PyObject * pluginObj;

    PyObject * perfBeginCB = NULL;
    PyObject * perfMarkerCB = NULL;
    PyObject * perfCompleteCB = NULL;
    PyObject * userArg = NULL;

    perf_plugin_callback_bucket_t * callbackBucket;
    PyObject * callbackObj = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = "";

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OOOO", &perfBeginCB, &perfMarkerCB, &perfCompleteCB, &userArg)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // create memory for the globus_ftp_client_plugin_t
    pluginp = (globus_ftp_client_plugin_t *) globus_malloc(sizeof(globus_ftp_client_plugin_t));

    // create a callback struct to hold the callback information
    callbackBucket = (perf_plugin_callback_bucket_t *) globus_malloc(sizeof(perf_plugin_callback_bucket_t));
    callbackBucket -> begincb = perfBeginCB;
    callbackBucket -> markercb = perfMarkerCB;
    callbackBucket -> completecb = perfCompleteCB;
    callbackBucket -> userarg = userArg;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> begincb);
    Py_XINCREF(callbackBucket -> markercb);
    Py_XINCREF(callbackBucket -> completecb);
    Py_XINCREF(callbackBucket -> userarg);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_perf_plugin_init(
        pluginp,
        perf_plugin_begin_cb,
        perf_plugin_marker_cb,
        perf_plugin_complete_cb,
        callbackBucket);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to initialize perf plugin", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }
    

    // wrap pointer to plugin and callback struct and return
    pluginObj = PyCObject_FromVoidPtr((void *) pluginp, NULL);
    callbackObj = PyCObject_FromVoidPtr((void *) callbackBucket, NULL);

    return Py_BuildValue("(OO)", (void *) pluginObj, (void *) callbackObj);

}

// destroy a previously created performance marker plugin and the
// callback that was created at the same time
PyObject * gridftp_perf_plugin_destroy(PyObject *self, PyObject *args)
{
    globus_ftp_client_plugin_t * pluginp = NULL;
    PyObject * pluginObj;
    PyObject * callbackObj;
    perf_plugin_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = "";

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OO", &pluginObj, &callbackObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

   // obtain the C pointers from the Python pointers 
   pluginp = (globus_ftp_client_plugin_t *) PyCObject_AsVoidPtr(pluginObj);
   callbackBucket = (perf_plugin_callback_bucket_t *) PyCObject_AsVoidPtr(callbackObj);
 

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_perf_plugin_destroy(pluginp);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to destroy perf plugin", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // free the memory used by the plugin
    globus_free(pluginp);

    // free the memory used by the perf callback struct
    globus_free(callbackBucket);

    // return None to indicate success
    Py_RETURN_NONE;
}

// add a plugin to a handle
PyObject * gridftp_handle_add_plugin(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    globus_ftp_client_plugin_t * pluginp = NULL;
    PyObject * handleObj;
    PyObject * pluginObj;

    globus_result_t gridftp_result;
    char msg[2048] = "";

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OO", &handleObj, &pluginObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

   // obtain the C pointers from the Python pointers 
   handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
   pluginp = (globus_ftp_client_plugin_t *) PyCObject_AsVoidPtr(pluginObj);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handle_add_plugin(handlep, pluginp);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to add plugin to handle", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// remove a plugin from a handle
PyObject * gridftp_handle_remove_plugin(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    globus_ftp_client_plugin_t * pluginp = NULL;
    PyObject * handleObj;
    PyObject * pluginObj;

    globus_result_t gridftp_result;
    char msg[2048] = "";

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OO", &handleObj, &pluginObj)){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }

   // obtain the C pointers from the Python pointers 
   handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
   pluginp = (globus_ftp_client_plugin_t *) PyCObject_AsVoidPtr(pluginObj);

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_handle_remove_plugin(handlep, pluginp);

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to remove plugin from handle", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}

// check for existence of a file or directory, i.e. a URL
// the status is returned in a callback
PyObject * gridftp_exists(PyObject *self, PyObject *args)
{
    globus_ftp_client_handle_t * handlep = NULL;
    char * url = NULL;
    globus_ftp_client_operationattr_t * operation_attrp = NULL;

    PyObject * handleObj;
    PyObject * OpAttrObj;
    PyObject * completeCallbackFunctionObj;
    PyObject * completeCallbackArgObj;

    exists_callback_bucket_t * callbackBucket = NULL;

    globus_result_t gridftp_result;
    char msg[2048] = ""; 

    // get Python arguments
    if (!PyArg_ParseTuple(args, "OsOOO", 
            &handleObj, 
            &url, 
            &OpAttrObj,
            &completeCallbackFunctionObj,
            &completeCallbackArgObj
            )){
        PyErr_SetString(PyExc_RuntimeError, "gridftpwrapper: unable to parse arguments");
        return NULL;
    }
 
    // get the bare pointers from the python objects
    handlep = (globus_ftp_client_handle_t *) PyCObject_AsVoidPtr(handleObj);
    operation_attrp = (globus_ftp_client_operationattr_t *) PyCObject_AsVoidPtr(OpAttrObj);

    // create an exists callback struct to hold the callback information
    callbackBucket = (exists_callback_bucket_t *) globus_malloc(sizeof(exists_callback_bucket_t));
    callbackBucket -> pyfunction = completeCallbackFunctionObj;
    callbackBucket -> pyarg = completeCallbackArgObj;

    // since we are holding pointers to these objects we need to increase
    // the reference count for each
    Py_XINCREF(callbackBucket -> pyfunction);
    Py_XINCREF(callbackBucket -> pyarg);

    // kick off the exists operation

    Py_BEGIN_ALLOW_THREADS

    gridftp_result = globus_ftp_client_exists(
		         handlep,
                        url,
                        operation_attrp,
                        exists_complete_callback,
                        (void *) callbackBucket
                        );

    Py_END_ALLOW_THREADS

    if (gridftp_result != GLOBUS_SUCCESS){
        sprintf(msg, "gridftpwrapper: rc = %d: unable to start exists operation", gridftp_result);
        PyErr_SetString(PyExc_RuntimeError, msg);
        return NULL;
    }

    // return None to indicate success
    Py_RETURN_NONE;

}



//
// This section of the code is for details needed to
// make this wrapping a Python module.
//
//
//
//
//

// method table mapping names to wrappers
static PyMethodDef gridftpwrappermethods[] = {
    {"gridftp_modules_activate", gridftp_modules_activate, METH_VARARGS},
    {"gridftp_modules_deactivate", gridftp_modules_deactivate, METH_VARARGS},
    {"gridftp_handleattr_init", gridftp_handleattr_init, METH_VARARGS},
    {"gridftp_handle_init", gridftp_handle_init, METH_VARARGS},
    {"gridftp_handleattr_destroy", gridftp_handleattr_destroy, METH_VARARGS},
    {"gridftp_handleattr_set_cache_all", gridftp_handleattr_set_cache_all, METH_VARARGS},
    {"gridftp_handle_destroy", gridftp_handle_destroy, METH_VARARGS},
    {"gridftp_operationattr_init", gridftp_operationattr_init, METH_VARARGS},
    {"gridftp_operationattr_destroy", gridftp_operationattr_destroy, METH_VARARGS},
    {"gridftp_operationattr_set_mode", gridftp_operationattr_set_mode, METH_VARARGS},
    {"gridftp_operationattr_set_disk_stack", gridftp_operationattr_set_disk_stack, METH_VARARGS},
    {"gridftp_operationattr_set_parallelism", gridftp_operationattr_set_parallelism, METH_VARARGS},
    {"gridftp_operationattr_set_tcp_buffer", gridftp_operationattr_set_tcp_buffer, METH_VARARGS},
    {"gridftp_parallelism_init", gridftp_parallelism_init, METH_VARARGS},
    {"gridftp_parallelism_destroy", gridftp_parallelism_destroy, METH_VARARGS},
    {"gridftp_parallelism_set_mode", gridftp_parallelism_set_mode, METH_VARARGS},
    {"gridftp_parallelism_set_size", gridftp_parallelism_set_size, METH_VARARGS},
    {"gridftp_tcpbuffer_init", gridftp_tcpbuffer_init, METH_VARARGS},
    {"gridftp_tcpbuffer_destroy", gridftp_tcpbuffer_destroy, METH_VARARGS},
    {"gridftp_tcpbuffer_set_mode", gridftp_tcpbuffer_set_mode, METH_VARARGS},
    {"gridftp_tcpbuffer_set_size", gridftp_tcpbuffer_set_size, METH_VARARGS},
    {"gridftp_third_party_transfer", gridftp_third_party_transfer, METH_VARARGS},
    {"gridftp_cksm", gridftp_cksm, METH_VARARGS},
    {"gridftp_mkdir", gridftp_mkdir, METH_VARARGS},
    {"gridftp_rmdir", gridftp_rmdir, METH_VARARGS},
    {"gridftp_delete", gridftp_delete, METH_VARARGS},
    {"gridftp_move", gridftp_move, METH_VARARGS},
    {"gridftp_chmod", gridftp_chmod, METH_VARARGS},
    {"gridftp_exists", gridftp_exists, METH_VARARGS},
    {"gridftp_get", gridftp_get, METH_VARARGS},
    {"gridftp_verbose_list", gridftp_verbose_list, METH_VARARGS},
    {"gridftp_register_read", gridftp_register_read, METH_VARARGS},
    {"gridftp_create_buffer", gridftp_create_buffer, METH_VARARGS},
    {"gridftp_destroy_buffer", gridftp_destroy_buffer, METH_VARARGS},
    {"gridftp_buffer_to_string", gridftp_buffer_to_string, METH_VARARGS},
    {"gridftp_abort", gridftp_abort, METH_VARARGS},
    {"gridftp_perf_plugin_init", gridftp_perf_plugin_init, METH_VARARGS},
    {"gridftp_perf_plugin_destroy", gridftp_perf_plugin_destroy, METH_VARARGS},
    {"gridftp_handle_add_plugin", gridftp_handle_add_plugin, METH_VARARGS},
    {"gridftp_handle_remove_plugin", gridftp_handle_remove_plugin, METH_VARARGS},
    {NULL, NULL}
};


// module initialization function
void initgridftpwrapper(){
    PyObject * module;
    PyObject * moduleDict;

    // Since the module initialization is done by the main thread (the
    // one servicing the Python interpreter) we need to call this
    // initialization function before any other C threads are created,
    // for example by the Globus libraries, so that those other threads
    // call later work properly with Python (for example so that the
    // callbacks can call into Python to invoke the Python level callbacks
    //
    // Here is what the Python C API doc says for this function:
    //
    // Initialize and acquire the global interpreter lock. It should be 
    // called in the main thread before creating a second thread
    // or engaging in any other thread operations...this is a no-op when called 
    // for a second time. 
    PyEval_InitThreads();

    // initialize the necessary Globus modules
    gridftp_modules_activate(NULL, NULL);

    // get handle to the module dictionary
    module = Py_InitModule("gridftpwrapper", gridftpwrappermethods);
    moduleDict = PyModule_GetDict(module);

    // populate the module dictionary with useful constants
    PyDict_SetItemString(moduleDict, "GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK", Py_BuildValue("i", (int) GLOBUS_FTP_CONTROL_MODE_EXTENDED_BLOCK));
    PyDict_SetItemString(moduleDict, "GLOBUS_FTP_CONTROL_PARALLELISM_FIXED", Py_BuildValue("i", (int) GLOBUS_FTP_CONTROL_PARALLELISM_FIXED));
    PyDict_SetItemString(moduleDict, "GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED", Py_BuildValue("i", (int) GLOBUS_FTP_CONTROL_TCPBUFFER_FIXED));

}
