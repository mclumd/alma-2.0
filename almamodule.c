#define PY_SSIZE_T_CLEAN
#include <Python.h>
//#include <arrayobject.h>
#include "alma_command.h"
#include "alma_kb.h"
#include "alma_print.h"
#include "tommy.h"

#if PY_MAJOR_VERSION >= 3
#define PY3
#endif

#define BUF_LIMIT 1000000
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
static PyObject * alma_init(PyObject *self, PyObject *args);

static PyObject *alma_term_to_pyobject(kb *collection, alma_term *term) {
  PyObject *ret_val;
  if (term->type == VARIABLE)
    ret_val = Py_BuildValue("[s,s,i]","var",term->variable->name, term->variable->id);
  //    tee_alt("%s%lld", collection, buf, term->variable->name, term->variable->id);
  else if (term->type == FUNCTION)
    ret_val = alma_function_to_pyobject(collection, term->function);
  else
    ret_val = alma_quote_to_pyobject(collection, term->quote);

  return ret_val;
}

static PyObject *alma_function_to_pyobject(kb *collection, alma_function *func) {
  PyObject *ret_val, *temp;
  //  tee_alt("%s", collection, buf, func->name);
  temp = Py_BuildValue("[]");
  if (func->term_count > 0) {
    //tee_alt("(", collection, buf);
    for (int i = 0; i < func->term_count; i++) {
      //      if (i > 0)
      //        tee_alt(", ", collection, buf);
      PyList_Append(temp,alma_term_to_pyobject(collection, func->terms + i));
    }
    //    tee_alt(")", collection, buf);
  }
  ret_val = Py_BuildValue("[s,s,O]","func",func->name,temp);
  
  return ret_val;
}

static PyObject *alma_quote_to_pyobject(kb *collection, alma_quote *quote) {
  PyObject *ret_val, *temp;

  //  tee_alt("\"", collection, buf);
  if (quote->type == SENTENCE)
    temp = alma_fol_to_pyobject(collection, quote->sentence);
  else
    temp = clause_to_pyobject(collection, quote->clause_quote);
  //  tee_alt("\"", collection, buf);
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
    } else {
      temp1 = Py_BuildValue("[]");
    }
    //    tee_alt("(", collection, buf);
    //    if (node->fol->op == NOT)
    //      tee_alt("%s", collection, buf, op);
    PyList_Append(temp1,alma_fol_to_pyobject(collection, node->fol->arg1));

    //    PyList_Append(ret_val,temp1);
    if (node->fol->arg2 != NULL) {
      //      tee_alt(" %s ", collection, buf, op);
      PyList_Append(temp1,alma_fol_to_pyobject(collection, node->fol->arg2));
    }
    if (node->fol->op != IF) {
      PyList_Append(ret_val,temp1);
    }
    //    tee_alt(")", collection, buf);
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
    //    if (negate)
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

static PyObject *clause_to_pyobject(kb *collection, clause *c) {
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
	//        tee_alt("~", collection, buf);
        temp2 = alma_function_to_pyobject(collection, f);
	temp2 = Py_BuildValue("[s,O]","neg",temp2);
      }
      //      if (i < c->fif->premise_count-1)
      //        tee_alt(" /\\", collection, buf);
      //      tee_alt(" ", collection, buf);
      PyList_Append(temp1,temp2);
    }
    temp1 = Py_BuildValue("[s,O]","and",temp1);
    PyList_Append(ret_val,temp1);
    //    tee_alt("-f-> ", collection, buf);
    temp1 = alma_function_to_pyobject(collection, c->fif->conclusion);
    if (c->fif->neg_conc)
      //tee_alt("~", collection, buf);
      temp1 = Py_BuildValue("[s,O]","neg",temp1);

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
      //      if (c->tag == BIF)
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





static PyObject *alma_set_priorities(PyObject *self, PyObject *args) {
  PyObject *subject_name_list;
  PyObject *subject_priority_list;
  PyObject *list_item;
  Py_ssize_t list_len;
  int idx;
  long alma_kb;
  kb *collection;
  //tommy_array *collection_subjects;
  //double *collection_priorities;
  char *subj_copy;
  int subj_len;

  if (!PyArg_ParseTuple(args, "lOO", &alma_kb, &subject_name_list, &subject_priority_list )) {
    return NULL;
  }

  collection = (kb *) alma_kb;
  //collection_subjects = collection->subject_list;
  //collection_priorities = collection->subject_priorities;

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
      //fprintf(stderr, "Got priority %f\n", PyFloat_AsDouble(list_item));
      collection->subject_priorities[idx] =  PyFloat_AsDouble(list_item);
      }
  }
  return Py_None;
}

/* 
   Expects as input a pointer to a knowledge base and a list of doubles with cardinality the number of resolutions in the pre-resolution buffer.  Sets the correspoding priorites accordingly. 
*/


static PyObject *set_prb_priorities(PyObject *self, PyObject *args) {
 PyObject *list_item;
  Py_ssize_t list_len;
  int idx;
  long alma_kb;
  kb *collection;
  tommy_list *collection_prb;
  PyObject *priority_list;
  //char *subj_copy;
  //int subj_len;
  double priority;
  tommy_node *prb_elmnt;
  

  if (!PyArg_ParseTuple(args, "lO", &alma_kb, &priority_list )) {
    return NULL;
  }

  collection = (kb *) alma_kb;
  collection_prb = &collection->pre_res_task_buffer;

  list_len = PyList_Size(priority_list);
  prb_elmnt = tommy_list_head(collection_prb);

  
  for (idx = 0; idx < list_len; idx++) {
    list_item = PyList_GetItem(priority_list, idx);
    if (!PyFloat_Check(list_item)) {
      PyErr_SetString(PyExc_TypeError, "list items must be double.");
    } else {
      //fprintf(stderr, "Got priority %f\n", PyFloat_AsDouble(list_item));
      priority = PyFloat_AsDouble(list_item);
      pre_res_task *data = prb_elmnt->data;
      data->priority = priority;
      prb_elmnt = prb_elmnt->next;
    }
  }
  return Py_None;
}

static PyObject *prb_to_resolutions(PyObject *self, PyObject *args) {
  long alma_kb;
  kb *collection;

  //fprintf(stderr, "In prb_to_resolutions\n");
  if (!PyArg_ParseTuple(args, "l", &alma_kb )) {
    return NULL;
  }
  collection = (kb *)alma_kb;
  pre_res_buffer_to_heap(collection);
  return Py_None;
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

  //fprintf(stderr, "In alma_to_pyobject\n");
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
  char *ret_val;

  //fprintf(stderr, "In alma_step\n");
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

  //fprintf(stderr, "In alma_atomic_step\n");
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  //fprintf(stderr, "atomic step\n");
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



static PyObject *get_res_buf( PyObject *self, PyObject *args) {
  //Get resolution task heap.

  long alma_kb;
  PyObject *py_lst;
  kb *collection;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  py_lst = Py_BuildValue("[]");
  collection = (kb *)alma_kb;


  res_task_heap *res_tasks = &collection->res_tasks;
  res_task_pri *element;
  res_task *t;
  char *clauses_string;

  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  

  
  for ( size_t i=0; i < res_tasks->count; i++) {
    element = res_task_heap_item(res_tasks, i);   // Type (res_task_pri *)
    t = element->res_task;

    alma_function_print(collection, t->pos, &buf);
    tee_alt("\t", collection, &buf);
    alma_function_print(collection, t->neg, &buf);
    tee_alt("\n", collection, &buf);

    PyList_Append(py_lst, Py_BuildValue("OOd",
					clause_to_pyobject(collection, t->x),
					clause_to_pyobject(collection, t->y),
					element->priority));
  }
  

  clauses_string = malloc(buf.size + 1);
  strcpy(clauses_string, buf.buffer);
  clauses_string[buf.size] = '\0';
  free(buf.buffer);
  return Py_BuildValue("(O,s)",py_lst, clauses_string);
}


// Returns the clauses and priorities in the pre-resolution-task
// buffer and a separate list of the positive/negative pairs within
// them.

static PyObject *alma_get_pre_res_task_buffer( PyObject *self, PyObject *args) {
  long alma_kb;
  PyObject *py_lst;
  PyObject *resolvent_lst;
  kb *collection;
  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  py_lst = Py_BuildValue("[]");
  resolvent_lst  = Py_BuildValue("[]");
  collection = (kb *)alma_kb;


  res_task *t;
  char *clauses_string;

  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  


  tommy_node *i = tommy_list_head(&collection->pre_res_task_buffer);
  while (i) {
    pre_res_task *data = i->data;
    t = data->t;


    alma_function_print(collection, t->pos, &buf);
    tee_alt("\t", collection, &buf);
    alma_function_print(collection, t->neg, &buf);
    tee_alt("\n", collection, &buf);

    
    PyList_Append(resolvent_lst, Py_BuildValue("OO",
					       alma_function_to_pyobject(collection, t->pos),
					       alma_function_to_pyobject(collection, t->neg)));
    PyList_Append(py_lst, Py_BuildValue("OOd",
					clause_to_pyobject(collection, t->x),
					clause_to_pyobject(collection, t->y),
					data->priority));

    i = i->next;
  }
  

  clauses_string = malloc(buf.size + 1);
  strcpy(clauses_string, buf.buffer);
  clauses_string[buf.size] = '\0';
  free(buf.buffer);
  return Py_BuildValue("(O,O, s)",py_lst, resolvent_lst, clauses_string);
}
  



static PyObject * alma_kbprint(PyObject *self, PyObject *args) {
  char *ret_val;
  long alma_kb;
  
  //fprintf(stderr, "In alma_kbprint\n");  
  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;


  kb_str buf;
  buf.size = 0;
  buf.limit = BUF_LIMIT;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';
  buf.curr = buf.buffer;
  kb_print((kb *)alma_kb, &buf);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

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

  //fprintf(stderr, "In alma_add\n");  
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

  //fprintf(stderr, "deleting %s -- len==%d  ....   ", input, len);
  assertion = malloc(len + 1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_remove((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  //fprintf(stderr, "done.\n");
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
  //fprintf(stderr, "updating %s -- len==%d  ....   ", input, len);
  
  assertion = malloc(len + 1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_update((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  //fprintf(stderr, " done.\n");
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

  //fprintf(stderr, "observing %s -- len==%d  ....   ", input, len);
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

  //fprintf(stderr, " done.\n");
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

  //fprintf(stderr, "bsing %s -- len==%d  ....   ", input, len);
  assertion = malloc(len+1);
  strncpy(assertion, input, len);
  assertion[len] = '\0';
  kb_backsearch((kb *)alma_kb, assertion, &buf);
  free(assertion);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);

  //fprintf(stderr, " done.\n");
  return Py_BuildValue("s",ret_val);
}

static PyMethodDef AlmaMethods[] = {
  {"init", alma_init, METH_VARARGS,"Initialize an alma kb."},
  {"kb_to_pyobject", alma_to_pyobject, METH_VARARGS,"Return python list representation of alma kb."},
  {"mode", alma_mode, METH_VARARGS,"Check python mode."},
  {"step", alma_step, METH_VARARGS,"Step an alma kb."},
  {"astep", alma_atomic_step, METH_VARARGS,"Step an alma kb, atomically."},
  {"prebuf", alma_get_pre_res_task_buffer, METH_VARARGS,"Retrieve pre-resolution task list."},
  {"set_priors_prb", set_prb_priorities, METH_VARARGS,"Set pre-resolution task list priorities."},
  {"prb_to_res_task", prb_to_resolutions, METH_VARARGS,"Flush pre-resolution task list to resolution task heap."},
  {"res_task_buf", get_res_buf, METH_VARARGS,"Get resolution task heap."},
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


// Placed at the end only because macros mess up formatting
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
	//fprintf(stderr, "Got priority %f\n", PyFloat_AsDouble(list_item));
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
    rip.use_res_pre_buffer = 1;
  } else {
    fprintf(stderr, "Cannot track resolution priorites using files; disabling resolution tracking.\n");
    rip.use_lists = 0;
  }


  
  kb_init(&alma_kb,file,agent,verbose, differential_priorities, res_heap_size, rip,  &buf, 0);

  ret_val = malloc(buf.size + 1);
  strcpy(ret_val,buf.buffer);
  ret_val[buf.size] = '\0';
  free(buf.buffer);
  
  return Py_BuildValue("(l,s),",(long)alma_kb,ret_val);
  }


/*

// Python2
PyMODINIT_FUNC
initalma(void)
{
  (void) Py_InitModule("alma", AlmaMethods);
}

*/

