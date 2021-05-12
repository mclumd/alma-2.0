#include <Python.h>
//#include <arrayobject.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"
#include "alma_fif.h"
#include "tommy.h"

extern char python_mode;
extern char logs_on;

static PyObject *alma_term_to_pyobject(kb *collection, alma_term *term);
static PyObject *alma_function_to_pyobject(kb *collection, alma_function *func);
static PyObject *alma_quote_to_pyobject(kb *collection, alma_quote *quote);
static PyObject *alma_fol_to_pyobject(kb *collection, alma_node *node);
static PyObject *lits_to_pyobject(kb *collection, alma_function **lits, int count, char *delimiter, int negate);
static PyObject * clause_to_pyobject(kb *collection, clause *c);

static PyObject *alma_term_to_pyobject(kb *collection, alma_term *term) {
  PyObject *ret_val;
  if (term->type == VARIABLE)
    ret_val = Py_BuildValue("[s,s,i]","var",term->variable->name, term->variable->id);
  //tee_alt("%s%lld", collection, buf, term->variable->name, term->variable->id);
  else if (term->type == FUNCTION)
    ret_val = alma_function_to_pyobject(collection, term->function);
  else
    ret_val = alma_quote_to_pyobject(collection, term->quote);

  return ret_val;
}

static PyObject *alma_function_to_pyobject(kb *collection, alma_function *func) {
  PyObject *ret_val, *temp;
  //tee_alt("%s", collection, buf, func->name);
  temp = Py_BuildValue("[]");
  if (func->term_count > 0) {
    //tee_alt("(", collection, buf);
    for (int i = 0; i < func->term_count; i++) {
      //if (i > 0)
      //tee_alt(", ", collection, buf);
      PyList_Append(temp,alma_term_to_pyobject(collection, func->terms + i));
    }
    //tee_alt(")", collection, buf);
  }
  ret_val = Py_BuildValue("[s,s,O]","func",func->name,temp);
  
  return ret_val;
}

static PyObject *alma_quote_to_pyobject(kb *collection, alma_quote *quote) {
  PyObject *ret_val, *temp;

  //tee_alt("\"", collection, buf);
  if (quote->type == SENTENCE)
    temp = alma_fol_to_pyobject(collection, quote->sentence);
  else
    temp = clause_to_pyobject(collection, quote->clause_quote);
  //tee_alt("\"", collection, buf);
  ret_val = Py_BuildValue("[s,O]","quote",temp);

  return ret_val;
}

static PyObject *alma_fol_to_pyobject(kb *collection, alma_node *node) {
  PyObject *ret_val, *temp1;
  if (node->type == FOL) {
    char *op;
    switch (node->fol->op) {
      case NOT:
        op = "neg"; break;
      case OR:
        op = "or"; break;
      case AND:
        op = "and"; break;
      case IF:
      default:
        if (node->fol->tag == FIF)
          op = "fif";
        else if (node->fol->tag == BIF)
          op = "bif";
        else
          op = "if";
        break;
    }

    ret_val = Py_BuildValue("[s]",op);
    if (node->fol->op == IF) {
      temp1 = ret_val;
    }
    else {
      temp1 = Py_BuildValue("[]");
    }
    //tee_alt("(", collection, buf);
    //if (node->fol->op == NOT)
    //  tee_alt("%s", collection, buf, op);
    PyList_Append(temp1,alma_fol_to_pyobject(collection, node->fol->arg1));

    //PyList_Append(ret_val,temp1);
    if (node->fol->arg2 != NULL) {
      //tee_alt(" %s ", collection, buf, op);
      PyList_Append(temp1,alma_fol_to_pyobject(collection, node->fol->arg2));
    }
    if (node->fol->op != IF) {
      PyList_Append(ret_val,temp1);
    }
    //tee_alt(")", collection, buf);
  }
  else
    ret_val = alma_function_to_pyobject(collection, node->predicate);

  return ret_val;
}

static PyObject *lits_to_pyobject(kb *collection, alma_function **lits, int count, char *delimiter, int negate) {
  PyObject *retval, *temp1, *temp2 = NULL;
  int i;
  
  if (count > 1)
    retval = Py_BuildValue("[s]",delimiter);
  else
    retval = NULL;
  temp1 = Py_BuildValue("[]");
  for (i = 0; i < count; i++) {
    //if (negate)
    //  tee_alt("~", collection, buf);
    temp2 = alma_function_to_pyobject(collection, lits[i]);
    if (negate)
      temp2 = Py_BuildValue("[s,O]","neg",temp2);
    //if (i < count-1)
    //  tee_alt(" %s ", collection, buf, delimiter);
    PyList_Append(temp1,temp2);
  }
  if (retval)
    PyList_Append(retval,temp1);
  else
    retval = temp2;
  return retval;
}

static PyObject * clause_to_pyobject(kb *collection, clause *c) {
  // Print fif in original format
  PyObject *ret_val = NULL;
  PyObject *temp1, *temp2;
  int i;
  
  if (c->tag == FIF) {
    ret_val = Py_BuildValue("[s]","fif");
    temp1 = Py_BuildValue("[]");
    for (i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        temp2 = alma_function_to_pyobject(collection, f);
      }
      else {
        //tee_alt("~", collection, buf);
        temp2 = alma_function_to_pyobject(collection, f);
	      temp2 = Py_BuildValue("[s,O]","neg",temp2);
      }
      //if (i < c->fif->premise_count-1)
      //  tee_alt(" /\\", collection, buf);
      //tee_alt(" ", collection, buf);
      PyList_Append(temp1,temp2);
    }
    temp1 = Py_BuildValue("[s,O]","and",temp1);
    PyList_Append(ret_val,temp1);
    //tee_alt("-f-> ", collection, buf);
    temp1 = alma_function_to_pyobject(collection, c->fif->conclusion);
    if (c->fif->neg_conc) {
      //tee_alt("~", collection, buf);
      temp1 = Py_BuildValue("[s,O]","neg",temp1);
    }

    PyList_Append(ret_val,temp1);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        ret_val = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "and", 0); //"/\\", 0);
        //tee_alt(" -b-> F", collection, buf);
	      ret_val = Py_BuildValue("[s,O,[s]]","bif",ret_val,"F");
      }
      else
        ret_val = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "or", 1); //"\\/", 1);
    }
    else if (c->neg_count == 0) {
      //if (c->tag == BIF)
      //  tee_alt("T -b-> ", collection, buf);
      ret_val = lits_to_pyobject(collection, c->pos_lits, c->pos_count, "or", 0); //"\\/", 0);
      if (c->tag == BIF)
	      ret_val = Py_BuildValue("[s,[s],O]","bif","T",ret_val);
    }
    else {
      temp1 = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "and", 0); //"/\\", 0);
      //tee_alt(" -%c-> ", collection, buf, c->tag == BIF ? 'b' : '-');
      temp2 = lits_to_pyobject(collection, c->pos_lits, c->pos_count, "or", 0); //"\\/", 0);
      ret_val = Py_BuildValue("[s,O,O]",c->tag == BIF? "bif" : "if",temp1,temp2);
    }
  }

  c->pyobject_bit = (char) 0;

  return ret_val;
}


static PyObject * alma_init(PyObject *self, PyObject *args) {
  int verbose, log_mode;
  PyObject *ret_val;
  char *file;
  char *agent;
  char *trialnum;
  char *log_dir;
  
  if (!PyArg_ParseTuple(args, "iissss", &verbose, &log_mode, &file, &agent, &trialnum, &log_dir))
    return NULL;

  if (log_mode) {
    //printf("ENABLING LOGS!\n");
    enable_logs();
  }
  
  enable_python_mode();

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  kb *alma_kb;
  kb_init(&alma_kb,file,agent, trialnum, log_dir, verbose, &buf, log_mode); 

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';

  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("(l,s),",(long)alma_kb,buf.buffer);
  
  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_mode(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  return Py_BuildValue("i",(int)python_mode);
}

static PyObject * alma_to_pyobject(PyObject *self, PyObject *args) {
  PyObject *lst;
  long alma_kb;
  kb *collection;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;


  lst = Py_BuildValue("[]");
  collection = (kb *)alma_kb;
 
  tommy_node *i = tommy_list_head(&collection->clauses);
  while (i) {
    index_mapping *data = i->data;
    if (data->value->pyobject_bit || 1) { // remove || 1 to only send new elements added to kb
      PyList_Append(lst,clause_to_pyobject(collection, data->value));
    }
    i = i->next;
  }
  
  return lst;
}


static PyObject * alma_step(PyObject *self, PyObject *args) {
  long alma_kb;
  PyObject *ret_val;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  kb_step((kb *)alma_kb, &buf);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  
  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_print(PyObject *self, PyObject *args) {
  PyObject *ret_val;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 10;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  kb_print((kb *)alma_kb, &buf);

  //ret_val = malloc(buf.size + 1);
  //strncpy(ret_val,buf.buffer,buf.size);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  return ret_val;
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
  char *input;
  PyObject *ret_val;
  //char *assertion;
  //int len;
  long alma_kb;

  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  //printf("IN C ALMA_ADD\n");
  
  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  //len = strlen(input);
  
  //assertion = malloc(len);
  //strncpy(assertion, input, len);
  //assertion[len-1] = '\0';
  //kb_assert((kb *)alma_kb, assertion, &buf);
  kb_assert((kb *)alma_kb, input, &buf);
  //free(assertion);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_del(PyObject *self, PyObject *args) {
  char *input;
  PyObject *ret_val;
  //char *assertion;
  //int len;
  long alma_kb;
  //int error;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  //len = strlen(input);
  
  //assertion = malloc(len);
  //strncpy(assertion, input, len);
  //assertion[len-1] = '\0';
  //kb_remove((kb *)alma_kb, assertion, &buf);
  kb_remove((kb *)alma_kb, input, &buf);
  //free(assertion);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);


  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_update(PyObject *self, PyObject *args) {
  char *input;
  PyObject *ret_val;
  //char *assertion;
  //int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  //len = strlen(input);
  
  //assertion = malloc(len);
  //strncpy(assertion, input, len);
  //assertion[len - 1] = '\0';
  kb_update((kb *)alma_kb, input, &buf);
  //kb_update((kb *)alma_kb, assertion, &buf);
  //free(assertion);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_obs(PyObject *self, PyObject *args) {
  char *input;
  PyObject *ret_val;
  //char *assertion;
  //int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  //len = strlen(input);
  
  //assertion = malloc(len);
  //strncpy(assertion, input, len);
  //assertion[len-1] = '\0';
  kb_observe((kb *)alma_kb, input, &buf);
  //kb_observe((kb *)alma_kb, assertion, &buf);
  //free(assertion);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  return ret_val;
}

static PyObject * alma_bs(PyObject *self, PyObject *args) {
  char *input;
  PyObject *ret_val;
  //char *assertion;
  //int len;
  long alma_kb;
  
  if (!PyArg_ParseTuple(args, "ls", &alma_kb, &input))
    return NULL;

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  
  //len = strlen(input);
  
  //assertion = malloc(len);
  //strncpy(assertion, input, len);
  //assertion[len-1] = '\0';
  kb_backsearch((kb *)alma_kb, input, &buf);
  //kb_backsearch((kb *)alma_kb, assertion, &buf);
  //free(assertion);

  //ret_val = malloc(buf.size + 1);
  //strcpy(ret_val,buf.buffer);
  //ret_val[buf.size] = '\0';
  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  return ret_val;
}

static PyMethodDef AlmaMethods[] = {
  {"init", alma_init, METH_VARARGS,"Initialize an alma kb."},
  {"kb_to_pyobject", alma_to_pyobject, METH_VARARGS,"Return python list representation of alma kb."},
  {"mode", alma_mode, METH_VARARGS,"Check python mode."},
  {"step", alma_step, METH_VARARGS,"Step an alma kb."},
  {"kbprint", alma_print, METH_VARARGS,"Print out entire alma kb."},
  {"halt", alma_halt, METH_VARARGS,"Stop an alma kb."},
  {"add", alma_add, METH_VARARGS,"Add a clause or formula to an alma kb."},
  {"kbdel", alma_del, METH_VARARGS,"Delete a clause or formula from an alma kb."},
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
