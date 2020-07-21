#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"
#include "tommy.h"

#if PY_MAJOR_VERSION >= 3
#define PY3
#endif

#define BUF_LIMIT 1000000

extern char python_mode;

static PyObject * alma_init(PyObject *self, PyObject *args) {
  int verbose;
  int differential_priorities;
  int res_heap_size;
  char *ret_val;
  char *file;
  char *agent;
  rl_init_params rip;

  PyObject *list_item;
  PyObject *subject_name_list;
  PyObject *subject_priority_list;
  Py_ssize_t subj_list_len;
  
  int idx;
  kb *alma_kb;
  tommy_array *collection_subjects;
  double *collection_priorities;
  char *subj_copy;
  int subj_len;


  if (!PyArg_ParseTuple(args, "issiiOO", &verbose, &file, &agent, &differential_priorities, &res_heap_size, &subject_name_list, &subject_priority_list))
    return NULL;

  enable_python_mode();

  subj_list_len = PyList_Size(subject_name_list);
  collection_subjects = malloc(sizeof(tommy_array));
  tommy_array_init(collection_subjects);
  for (idx = 0; idx < subj_list_len; idx++) {
    list_item = PyList_GetItem(subject_name_list, idx);
#ifndef PY3
    if (!PyString_Check(list_item)) {
      PyErr_SetString(PyExc_TypeError, "list items must be strings.");
#else
    if (!PyUnicode_Check(list_item)) {
	PyErr_SetString(PyExc_TypeError, "list items must be strings.");
#endif
    } else {
      fprintf(stderr, "Got subject name %s\n", PyUnicode_AS_DATA(list_item));
#ifndef PY3
      subj_len = strlen(PyString_AsString(list_item)) + 1;
      subj_copy = malloc( subj_len * sizeof(char));
      strncpy(subj_copy, PyString_AsString(list_item), subj_len);
      subj_copy[subj_len-2] = '\0';
      tommy_array_insert(collection_subjects, subj_copy);
#else
      subj_len = strlen(PyUnicode_AS_DATA(list_item)) + 1;
      subj_copy = malloc( subj_len * sizeof(char));
      strncpy(subj_copy, PyUnicode_AS_DATA(list_item), subj_len);
      subj_copy[subj_len-1] = '\0';
      tommy_array_insert(collection_subjects, subj_copy);      
#endif
    }
    }
    
  assert(subj_list_len == PyList_Size(subject_priority_list));
  collection_priorities = malloc(sizeof(double)* subj_list_len);  
  for (idx = 0; idx < subj_list_len; idx++) {
    list_item = PyList_GetItem(subject_priority_list, idx);
    if (!PyFloat_Check(list_item)) {
      PyErr_SetString(PyExc_TypeError, "list items must be doubles.");
    } else {
      fprintf(stderr, "Got priority %f\n", PyFloat_AsDouble(list_item));
      collection_priorities[idx] =  PyFloat_AsDouble(list_item);
      }
  }


  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;

  rip.tracking_resolutions = 0;
  rip.subjects_file = NULL;
  rip.resolutions_file = NULL;
  rip.rl_priority_file = NULL;
  if (subj_list_len > 0) {
    rip.tracking_resolutions = 1;
    rip.tracking_subjects = collection_subjects;
    rip.tracking_priorities = collection_priorities;
    rip.use_lists = 1;
  } else {
    fprintf(stderr, "Cannot track resolution priorites using files; disabling resolution tracking.\n");
    rip.use_lists = 0;
  }
  

  
  kb_init(&alma_kb,file,agent,verbose, differential_priorities, res_heap_size, rip,  &buf); 

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("(l,s),",(long)alma_kb,ret_val);
  }



static PyObject *alma_set_priorities(PyObject *self, PyObject *args) {
  PyObject *subject_name_list;
  PyObject *subject_priority_list;
  PyObject *list_item;
  Py_ssize_t list_len;
  int idx;
  long alma_kb;
  kb *collection;
  tommy_array *collection_subjects;
  double *collection_priorities;
  char *subj_copy;
  int subj_len;

  if (!PyArg_ParseTuple(args, "lOO", &alma_kb, &subject_name_list, &subject_priority_list )) {
    return NULL;
  }

  collection = (kb *) alma_kb;
  collection_subjects = collection->subject_list;
  collection_priorities = collection->subject_priorities;
  
  list_len = PyList_Size(subject_name_list);
  tommy_array_init(collection->subject_list);
  for (idx = 0; idx < list_len; idx++) {
    list_item = PyList_GetItem(subject_name_list, idx);
    if (!PyUnicode_Check(list_item)) {
      PyErr_SetString(PyExc_TypeError, "list items must be strings.");
    } else {
      fprintf(stderr, "Got subject name %s\n", PyUnicode_AS_DATA(list_item));
      subj_len = strlen(PyUnicode_AS_DATA(list_item)) + 1;
      subj_copy = malloc( subj_len * sizeof(char));
      strcpy(subj_copy, PyUnicode_AS_DATA(list_item));
      tommy_array_insert(collection->subject_list, subj_copy);
    }
  }

  assert(list_len == PyList_Size(subject_priority_list));
  collection->subject_priorities = malloc(sizeof(double)* list_len);  
  for (idx = 0; idx < list_len; idx++) {
    list_item = PyList_GetItem(subject_priority_list, idx);
    if (!PyFloat_Check(list_item)) {
      PyErr_SetString(PyExc_TypeError, "list items must be doubles.");
    } else {
      fprintf(stderr, "Got priority %f\n", PyFloat_AsDouble(list_item));
      collection->subject_priorities[idx] =  PyFloat_AsDouble(list_item);
      }
  }
  return Py_None;
}

static PyObject * alma_mode(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  return Py_BuildValue("i",(int)python_mode);
}


static PyObject * alma_step(PyObject *self, PyObject *args) {
  long alma_kb;
  char *ret_val;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  //fprintf(stderr, "(full) stepping\n");
  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  kb_step((kb *)alma_kb, 0, &buf);

  ret_val = malloc( (buf.size + 1) * sizeof(char));
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  //fprintf("Stepping result:  %s\n", ret_val);
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_atomic_step(PyObject *self, PyObject *args) {
  long alma_kb;
  char *ret_val;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  fprintf(stderr, "atomic step\n");
  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  kb_step((kb *)alma_kb, 1, &buf);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_kbprint(PyObject *self, PyObject *args) {
  char *ret_val;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  fprintf(stderr, "printing\n");
  
  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  fprintf(stderr, "B\n");
  kb_print((kb *)alma_kb, &buf);
  fprintf(stderr, "C\n");
  
  ret_val = malloc(buf.size + 1);
  fprintf(stderr, "D\n");
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  fprintf(stderr, "E\n");
  
  return Py_BuildValue("(s,l)",ret_val,buf.size);
}

static PyObject * alma_halt(PyObject *self, PyObject *args) {
  long alma_kb;

  fprintf(stderr, "halting\n");
  if (!PyArg_ParseTuple(args, "l",&alma_kb))
    return NULL;

  kb_halt((kb *)alma_kb);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject * alma_add(PyObject *self, PyObject *args) {
  const char *input;
  char *ret_val;
  char *assertion;
  int len;
  long alma_kb;

  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

    
  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  //fprintf(stderr, "adding %s -- len==%d  ....   ", input, len);
  

  assertion = malloc(len+1);
  //strncpy(assertion, input, len);
  strcpy(assertion, input);
  //assertion[len] = '\0';
  kb_assert((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  //fprintf(stderr, "done.\n");
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_del(PyObject *self, PyObject *args) {
  const char *input;
  char *ret_val;
  char *assertion;
  int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;


  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);

  fprintf(stderr, "deleting %s -- len==%d  ....   ", input, len);
  assertion = malloc(len + 1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_remove((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  fprintf(stderr, "done.\n");
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_update(PyObject *self, PyObject *args) {
  const char *input;
  char *ret_val;
  char *assertion;
  int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  fprintf(stderr, "updating %s -- len==%d  ....   ", input, len);
  
  assertion = malloc(len + 1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_update((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  fprintf(stderr, " done.\n");
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_obs(PyObject *self, PyObject *args) {
  const char *input;
  char *ret_val;
  char *assertion;
  int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);

  fprintf(stderr, "observing %s -- len==%d  ....   ", input, len);
  assertion = malloc(len + 1);
  //strncpy(assertion, input, len);
  strcpy(assertion, input);
  assertion[len] = '\0';
  kb_observe((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  fprintf(stderr, " done.\n");
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_bs(PyObject *self, PyObject *args) {
  const char *input;
  char *ret_val;
  char *assertion;
  int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);

  fprintf(stderr, "bsing %s -- len==%d  ....   ", input, len);
  assertion = malloc(len+1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_backsearch((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  fprintf(stderr, " done.\n");
  return Py_BuildValue("s",ret_val);
}
 
static PyMethodDef AlmaMethods[] = {
  {"init", alma_init, METH_VARARGS,"Initialize an alma kb.  Param:  verbose, file, agent, differential_priorities, res_heap_size"},
  {"mode", alma_mode, METH_VARARGS,"Check python mode."},
  {"step", alma_step, METH_VARARGS,"Step an alma kb."},
  {"kbprint", alma_kbprint, METH_VARARGS,"Print out entire alma kb."},
  {"halt", alma_halt, METH_VARARGS,"Stop an alma kb."},
  {"add", alma_add, METH_VARARGS,"Add a clause or formula to an alma kb."},
  {"kbdel", alma_del, METH_VARARGS,"Delete a clause or formula from an alma kb."},
  {"update", alma_update, METH_VARARGS,"Update a clause or formula in an alma kb."},
  {"obs", alma_obs, METH_VARARGS,"Observe something in an alma kb."},
  {"bs", alma_bs, METH_VARARGS,"Backsearch an alma kb."},
  {"set_priorities", alma_set_priorities, METH_VARARGS, "Set subjects and priorities."},
  {NULL, NULL, 0, NULL}
};



static struct PyModuleDef almamodule = {
  PyModuleDef_HEAD_INIT,
  "alma",
  "Python interface for the alma C library",
  -1,
  AlmaMethods
};


PyMODINIT_FUNC PyInit_alma(void) {
  return PyModule_Create(&almamodule);
}


 
/*

// Python2
PyMODINIT_FUNC
initalma(void)
{
  (void) Py_InitModule("alma", AlmaMethods);
}

*/

