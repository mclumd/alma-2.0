#include <Python.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"

extern char python_mode;

static PyObject * alma_init(PyObject *self, PyObject *args) {
  int verbose;
  char *ret_val;
  char *file;
  char *agent;

  if (!PyArg_ParseTuple(args, "iss", &verbose, &file, &agent))
    return NULL;

  enable_python_mode();

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  kb *alma_kb;
  kb_init(&alma_kb,file,agent,verbose, &buf); 

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("(l,s),",(long)alma_kb,ret_val);
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

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  kb_step((kb *)alma_kb, &buf);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("s",ret_val);
}

static PyObject * alma_kbprint(PyObject *self, PyObject *args) {
  char *ret_val;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 10000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  kb_print((kb *)alma_kb, &buf);

  ret_val = malloc(buf.size + 1);
  strncpy(ret_val,buf.buffer,buf.size);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("(s,l)",ret_val,buf.size);
}

static PyObject * alma_halt(PyObject *self, PyObject *args) {
  long alma_kb;
  
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
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  
  assertion = malloc(len);
  strncpy(assertion, input, len);
  assertion[len-1] = '\0';
  kb_assert((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
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
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  
  assertion = malloc(len);
  strncpy(assertion, input, len);
  assertion[len-1] = '\0';
  kb_remove((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
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
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  
  assertion = malloc(len);
  strncpy(assertion, input, len);
  assertion[len-1] = '\0';
  kb_update((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
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
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  
  assertion = malloc(len);
  strncpy(assertion, input, len);
  assertion[len-1] = '\0';
  kb_observe((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
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
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  
  len = strlen(input);
  
  assertion = malloc(len);
  strncpy(assertion, input, len);
  assertion[len-1] = '\0';
  kb_backsearch((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("s",ret_val);
}

static PyMethodDef AlmaMethods[] = {
  {"init", alma_init, METH_VARARGS,"Initialize an alma kb."},
  {"mode", alma_mode, METH_VARARGS,"Check python mode."},
  {"step", alma_step, METH_VARARGS,"Step an alma kb."},
  {"kbprint", alma_kbprint, METH_VARARGS,"Print out entire alma kb."},
  {"halt", alma_halt, METH_VARARGS,"Stop an alma kb."},
  {"add", alma_add, METH_VARARGS,"Add a clause or formula to an alma kb."},
  {"del", alma_del, METH_VARARGS,"Delete a clause or formula from an alma kb."},
  {"update", alma_update, METH_VARARGS,"Update a clause or formula in an alma kb."},
  {"obs", alma_obs, METH_VARARGS,"Observe something in an alma kb."},
  {"bs", alma_bs, METH_VARARGS,"Backsearch an alma kb."},
  {NULL, NULL, 0, NULL}
};

/*
static struct PyModuleDef almamodule = {
  PyModuleDef_HEAD_INIT,
  "alma",
  "Python interface for the alma C library",
  -1,
  AlmaMethods
};
*/

PyMODINIT_FUNC
initalma(void)
{
  (void) Py_InitModule("alma", AlmaMethods);
}

/*
PyMODINIT_FUNC PyInit_alma(void) {
  (void) PyModule_Create(&almamodule);
}
*/
