#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <stdlib.h>
#include <assert.h>
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

//#define LOG(...) fprintf(stderr, __VA_ARGS__)
#define LOG(...)

extern char python_mode;
extern char logs_on;

int *memtest_array;

static PyObject *alma_term_to_pyobject(kb *collection, alma_term *term);
static PyObject *alma_function_to_pyobject(kb *collection, alma_function *func);
static PyObject *alma_quote_to_pyobject(kb *collection, alma_quote *quote);
static PyObject *alma_fol_to_pyobject(kb *collection, alma_node *node);
static PyObject *lits_to_pyobject(kb *collection, alma_function **lits, int count, char *delimiter, int negate);
static PyObject * clause_to_pyobject(kb *collection, clause *c);
static PyObject * alma_init(PyObject *self, PyObject *args);

/*  Some notes on memroy management:
    1.  Py_BuildValue copies strings before placing them into Python objects.  So it's not enough to free the original object, we also need to free the Python object.
        (source:  https://docs.python.org/3/c-api/arg.html)
    2.  Python methods which create Python objects will set the reference count to 1.  It is up to whoever owns the reference to decrement the reference count.
        (source:  https://pythonextensionpatterns.readthedocs.io/en/latest/refcount.html)
        a.  We might decrement the reference count from Python (maybe using del (?), or even just exiting from a function!)
	b.  We might just keep track of all returned reference in a global variable and decrement them on halt.

We'll go with 2a.   Some notes on reference counting (https://pythonextensionpatterns.readthedocs.io/en/latest/refcount.html):
   1.  References can be new, stolen or borrowed.
   2.  The "contract":

   Type           Contract

   New            Either you decref it or give it to someone who will, otherwise you have a memory leak.
   Stolen         The thief will take of things so you don’t have to. If you try to the results are undefined.
                  [ BUT, note that appending an object to a list increments it's reference count. ]
   Borrowed       The lender can invalidate the reference at any time
                  without telling you. Bad news. So increment a
                  borrowed reference whilst you need it and decrement
                  it when you are finished.

*/

static PyObject *alma_term_to_pyobject(kb *collection, alma_term *term) {
  PyObject *ret_val;
  if (term->type == VARIABLE) {
    ret_val = Py_BuildValue("[s,s,i]","var",term->variable->name, term->variable->id);
    LOG( "ckpt topy0.5:  ret_val created\n");
  //    tee_alt("%s%lld", collection, buf, term->variable->name, term->variable->id);
  } else if (term->type == FUNCTION)
    ret_val = alma_function_to_pyobject(collection, term->function);
  else
    ret_val = alma_quote_to_pyobject(collection, term->quote);
  LOG( "ckpt topy2:  ret_val refcount = %ld\n", Py_REFCNT(ret_val));
  return ret_val;
}

static PyObject *alma_function_to_pyobject(kb *collection, alma_function *func) {
  PyObject *ret_val = NULL;
  PyObject *temp = NULL ;
  PyObject *tmp2;
  //  tee_alt("%s", collection, buf, func->name);
  temp = Py_BuildValue("[]");

  /* DEBUG */
  //return temp;

  LOG( "ckpt func1:  temp ref count = %ld\n", Py_REFCNT(temp));
  if (func->term_count > 0) {
    //tee_alt("(", collection, buf);
    for (int i = 0; i < func->term_count; i++) {
      LOG( "i = %d \t", i);
      //      if (i > 0)
      //        tee_alt(", ", collection, buf);


      tmp2 = alma_term_to_pyobject(collection, func->terms + i);



      //Py_INCREF(tmp2);
      LOG( "ckpt func2:  temp ref count = %ld\n", Py_REFCNT(temp));
      LOG( "ckpt func2:  tmp2 refcount = %ld\n", Py_REFCNT(tmp2));
      PyList_Append(temp, tmp2);
      LOG( "ckpt func3:  temp ref count = %ld\n", Py_REFCNT(temp));
      LOG( "ckpt func4:  tmp2 refcount = %ld\n", Py_REFCNT(tmp2));
      Py_DECREF(tmp2);
      LOG( "ckpt func4.25:  tmp2 refcount = %ld\n", Py_REFCNT(tmp2));
    }
    //    tee_alt(")", collection, buf);
    }
  LOG( "ckpt func4.5:  temp ref count = %ld \t func->name: %s\n", Py_REFCNT(temp), func->name);
  ret_val = Py_BuildValue("[s,s,O]","func",func->name,temp);
  LOG( "ckpt func5:  temp ref count = %ld\n", Py_REFCNT(temp));
  LOG( "ckpt func5:  ret_val ref count = %ld\n", Py_REFCNT(ret_val));
  Py_DECREF(temp);  // temp's reference is stolen by ret_val, so it should be 1 on exit.
  LOG( "ckpt func6:  ret_val ref count = %ld\n", Py_REFCNT(ret_val));
  LOG( "ckpt func6:  temp ref count = %ld\n-----------------------------------------------------------------------------------------\n", Py_REFCNT(temp));
  return ret_val;
}


static PyObject *clause_to_pyobject(kb *collection, clause *c) {
  // Print fif in original format
  PyObject *ret_val = NULL;
  PyObject *temp1, *temp2;
  int i;

  LOG("co ckpt0\n");


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
      PyList_Append(temp1,temp2);
      Py_DECREF(temp2);
    }
    temp1 = Py_BuildValue("[s,O]","and",temp1);
    PyList_Append(ret_val,temp1);
    Py_DECREF(temp1);
    //    tee_alt("-f-> ", collection, buf);
    temp1 = alma_function_to_pyobject(collection, c->fif->conclusion);
    if (c->fif->neg_conc)
      //tee_alt("~", collection, buf);
      temp1 = Py_BuildValue("[s,O]","neg",temp1);

    PyList_Append(ret_val,temp1);
    Py_DECREF(temp1);
  } else {
    // Non-fif case
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        temp1 = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "and", 0); //"/\\", 0);
        //tee_alt(" -b-> F", collection, buf);
	ret_val = Py_BuildValue("[s,O,[s]]","bif",temp1,"F");
	Py_DECREF(temp1);
      }
      else {
	LOG( "ckpt co1\n");
        ret_val = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "or", 1); //"\\/", 1);
      }
    }
    else if (c->neg_count == 0) {
      //      if (c->tag == BIF)
      //  tee_alt("T -b-> ", collection, buf);
      //fprintf(stderr, "ckpt co noneg\n");

      /* DEBUG ONLY */
      //return Py_BuildValue("[s]", "non_neg");
      /* REMOVE */

      temp1 = lits_to_pyobject(collection, c->pos_lits, c->pos_count, "or", 0); //"\\/", 0);

      if (c->tag == BIF) {
	ret_val = Py_BuildValue("[s,[s],O]","bif","T",temp1);
	Py_DECREF(temp1);
      } else {
	ret_val = temp1;
	LOG( "temp1 refcount == %ld\n", Py_REFCNT(temp1));
	LOG( "retval refcount == %ld\n", Py_REFCNT(ret_val));
      }
      LOG("ckpt co noneg -- post-lits++\n");
    } else {
      temp1 = lits_to_pyobject(collection, c->neg_lits, c->neg_count, "and", 0); //"/\\", 0);
      //Py_INCREF(temp1);
      //tee_alt(" -%c-> ", collection, buf, c->tag == BIF ? 'b' : '-');
      LOG( "ckp co3 (pos_count = %d)\t \n", c->pos_count);
      temp2 = lits_to_pyobject(collection, c->pos_lits, c->pos_count, "or", 0); //"\\/", 0);
      //Py_INCREF(temp2);
      LOG( "ckp co3.5, temp1 refcount == %ld\n", Py_REFCNT(temp1));
      LOG( "ckp co3.5, temp2 refcount == %ld\n", Py_REFCNT(temp2));
      ret_val = Py_BuildValue("[s,O,O]",c->tag == BIF? "bif" : "if",temp1,temp2);
      LOG( "ckp co4, temp1 refcount == %ld\n", Py_REFCNT(temp1));
      LOG( "ckp co4, temp2 refcount == %ld\n", Py_REFCNT(temp2));
      Py_DECREF(temp1);
      Py_DECREF(temp2);
      LOG( "ckp co5, temp1 refcount == %ld\n", Py_REFCNT(temp1));
      LOG( "ckp co5, temp2 refcount == %ld\n", Py_REFCNT(temp2));
    }
  }
  c->pyobject_bit = (char) 0;
  LOG("ckpt co noneg -- post-lits++++\n");
  LOG( "ckp co6, ret_val refcount == %ld\n", Py_REFCNT(ret_val));
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
  // Py_DECREF(temp);   // temp's reference is stolen by ret_val

  return ret_val;
}

static PyObject *alma_fol_to_pyobject(kb *collection, alma_node *node) {
  PyObject *ret_val, *temp1;
  PyObject *tmp2;
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

    // An *IF* will be represented as ['if'/'fif'/'bif', antecedent list, consequent list].
    // Otherwise we have something of the form [op, fol list]
    ret_val = Py_BuildValue("[s]",op);
    if (node->fol->op == IF) {
      temp1 = ret_val;
    } else {
      temp1 = Py_BuildValue("[]");
    }
    //    tee_alt("(", collection, buf);
    //    if (node->fol->op == NOT)
    //      tee_alt("%s", collection, buf, op);
    tmp2 = alma_fol_to_pyobject(collection, node->fol->arg1);
    PyList_Append(temp1, tmp2);
    LOG( "ckpt fol1:  temp1 ref count = %ld\n", Py_REFCNT(temp1));
    LOG( "ckpt fol1:  tmp2 ref count = %ld\n", Py_REFCNT(tmp2));
    Py_DECREF(tmp2);
    LOG( "ckpt fol2:  tmp2 ref count = %ld\n", Py_REFCNT(tmp2));


    //    PyList_Append(ret_val,temp1);
    if (node->fol->arg2 != NULL) {   // This should only happen with one of the IF cases.
      //      tee_alt(" %s ", collection, buf, op);
      tmp2 = alma_fol_to_pyobject(collection, node->fol->arg2);
      PyList_Append(temp1, tmp2);
      LOG( "ckpt fol3:  temp1 ref count = %ld\n", Py_REFCNT(temp1));
      LOG( "ckpt fol3:  tmp2 ref count = %ld\n", Py_REFCNT(tmp2));
      Py_DECREF(tmp2);
      LOG( "ckpt fol3.1:  tmp2 ref count = %ld\n", Py_REFCNT(tmp2));
    }
    if (node->fol->op != IF) {
      PyList_Append(ret_val,temp1);
      Py_DECREF(temp1);   // stolen by ret_val
    }
    //    tee_alt(")", collection, buf);
  } else ret_val = alma_function_to_pyobject(collection, node->predicate);
  LOG( "ckpt fol4:  temp1 ref count = %ld\n", Py_REFCNT(temp1));
  LOG( "ckpt fol4:  ret_val ref count = %ld\n", Py_REFCNT(ret_val));
  return ret_val;
}

static PyObject *lits_to_pyobject(kb *collection, alma_function **lits, int count, char *delimiter, int negate) {
  PyObject *retval, *temp1, *temp2 = NULL;
  int i;

  assert(count > 0);
  // Returning early here fixes memory leak; issue is quite likely in this function. */
  if (count > 1)  {
    retval = Py_BuildValue("[s]",delimiter);
    temp1 = Py_BuildValue("[]");
    for (i = 0; i < count; i++) {
      temp2 = alma_function_to_pyobject(collection, lits[i]);

      if (negate)
	temp2 = Py_BuildValue("[s,O]","neg",temp2);

      PyList_Append(temp1,temp2);
      Py_DECREF(temp2);
    }

    PyList_Append(retval,temp1);
    LOG( "ckpt ltpy3:  temp1 ref %ld\n", Py_REFCNT(temp1));
    Py_DECREF(temp1);
    LOG( "ckpt ltpy4:  temp1 ref %ld\n", Py_REFCNT(temp1));
  } else {
    retval = NULL;
    temp1 = Py_BuildValue("[]");
    temp2 = alma_function_to_pyobject(collection, lits[0]);

    if (negate)
      temp2 = Py_BuildValue("[s,O]","neg",temp2);

    //PyList_Append(temp1,temp2);
    //Py_DECREF(temp2);
    retval = temp2;
  }

  return retval;
  
  temp1 = Py_BuildValue("[]");

  /*
     What happens to temp1 if count==1?   temp1 will be the list [temp2] and retval will be NULL.

 */
  for (i = 0; i < count; i++) {
    temp2 = alma_function_to_pyobject(collection, lits[i]);

    if (negate)
      temp2 = Py_BuildValue("[s,O]","neg",temp2);

    PyList_Append(temp1,temp2);
    Py_DECREF(temp2);
  }



  LOG( "ckpt ltpy2\n");
  if (retval) {
    PyList_Append(retval,temp1);
    LOG( "ckpt ltpy3:  temp1 ref %ld\n", Py_REFCNT(temp1));
    Py_DECREF(temp1);
    LOG( "ckpt ltpy4:  temp1 ref %ld\n", Py_REFCNT(temp1));
  }  else {
    //  This is the case for count ==1.   We still have to decref temp1
    fprintf(stderr, "count: %d \t temp1 ref %ld\n", count, Py_REFCNT(temp1));
    Py_DECREF(temp1);   // At this point, temp1 owns the reference to temp2 so there's a problem
    LOG( "ckpt ltpy5:  temp2 ref %ld\n", Py_REFCNT(temp2));
    retval = temp2;

    //Py_INCREF(retval);  // Moved from the end
    //Py_DECREF(temp2);  // Moved from the end
    LOG( "ckpt ltpy6:  temp2 ref %ld\n", Py_REFCNT(temp2));
    LOG( "ckpt ltpy6:  retval ref %ld\n", Py_REFCNT(retval));
    //Py_DECREF(temp2);   //Restore?
  }

  //LOG( "ckpt ltpy7:  temp1 ref %ld\n", Py_REFCNT(temp1));  LOG( "ckpt ltpy7.1:  temp1 ref %ld\n", Py_REFCNT(temp1));   LOG( "ckpt ltpy7.1:  temp2 ref %ld\n", Py_REFCNT(temp2));  LOG( "ckpt ltpy7.1:  retval ref %ld\n", Py_REFCNT(retval));

  //Py_DECREF(temp1);   // For some reason this causes a segfault

  
  return retval;
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
      //LOG( "Got subject name %s\n", PyUnicode_AS_DATA(list_item));
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
      //LOG( "Got priority %f\n", PyFloat_AsDouble(list_item));
      collection->subject_priorities[idx] =  PyFloat_AsDouble(list_item);
      }
  }
  Py_INCREF(Py_None);
  return Py_None;
}

tommy_list dummy_list;
static PyObject *alma_memtest_alloc(PyObject *self, PyObject *args) {

  tommy_list_init(&dummy_list);

  struct object {
    int value;
    // other fields
    tommy_node node;
  };
  for(int i=0; i < 10000000; i++) {
    //LOG( "%d ", i);
    struct object* obj = malloc(sizeof(struct object)); // creates the object
    obj->value = 0; // initializes the object
    tommy_list_insert_tail(&dummy_list, &obj->node, obj); // inserts the object
  }
    Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *alma_memtest_free(PyObject *self, PyObject *args) {
  tommy_list_foreach(&dummy_list, free);
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
      //LOG( "Got priority %f\n", PyFloat_AsDouble(list_item));
      priority = PyFloat_AsDouble(list_item);
      pre_res_task *data = prb_elmnt->data;
      data->priority = priority;
      prb_elmnt = prb_elmnt->next;
    }
  }
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *prb_to_resolutions(PyObject *self, PyObject *args) {
  long alma_kb;
  kb *collection;
  double threshold;

  //LOG( "In prb_to_resolutions\n");
  if (!PyArg_ParseTuple(args, "ld", &alma_kb, &threshold )) {
    return NULL;
  }
  collection = (kb *)alma_kb;
  collection->prb_threshold = threshold;
  pre_res_buffer_to_heap(collection, 0);
  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject *single_prb_to_resolutions(PyObject *self, PyObject *args) {
  long alma_kb;
  kb *collection;
  double threshold;

  //fprintf(stderr, "In prb_to_resolutions\n");
  if (!PyArg_ParseTuple(args, "ld", &alma_kb, &threshold )) {
        return NULL;
  }
  collection = (kb *)alma_kb;
  collection->prb_threshold = threshold;
  pre_res_buffer_to_heap(collection, 1);
  Py_INCREF(Py_None); // JB 5/25/21
  return Py_None;
}



static PyObject * alma_mode(PyObject *self, PyObject *args) {
  if (!PyArg_ParseTuple(args, ""))
    return NULL;

  return Py_BuildValue("i",(int)python_mode);
}

static PyObject *alma_to_pyobject(PyObject *self, PyObject *args) {
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
  PyObject *result;

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

  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //fprintf("Stepping result:  %s\n", ret_val);

  return result;
}

static PyObject * alma_atomic_step(PyObject *self, PyObject *args) {
  long alma_kb;
  //PyObject *result;

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

  // Don't really need to return anything
  //result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //return result;
  Py_INCREF(Py_None);
  return Py_None;
}



static PyObject *get_res_buf( PyObject *self, PyObject *args) {
  //Get resolution task heap.

  long alma_kb;
  PyObject *py_lst;
  kb *collection;
  PyObject *result;

  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  py_lst = Py_BuildValue("[]");
  collection = (kb *)alma_kb;


  res_task_heap *res_tasks = &collection->res_tasks;
  res_task_pri *element;
  res_task *t;

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



  result = Py_BuildValue("(O,s)",py_lst, buf.buffer);
  free(buf.buffer);
  return result;
}


// Returns the clauses and priorities in the pre-resolution-task
// buffer and a separate list of the positive/negative pairs within
// them.

static PyObject *alma_get_pre_res_task_buffer( PyObject *self, PyObject *args) {
  long alma_kb;
  PyObject *py_lst;
  PyObject *resolvent_lst;
  kb *collection;
  PyObject *result;
  PyObject *pos_term, *neg_term, *x, *y;
  PyObject *tmp;

  if (!PyArg_ParseTuple(args, "l", &alma_kb))
    return NULL;

  py_lst = Py_BuildValue("[]");


  resolvent_lst  = Py_BuildValue("[]");
  collection = (kb *)alma_kb;


  res_task *t;

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

    LOG( " ckpt 1\n");
    alma_function_print(collection, t->pos, &buf);
    tee_alt("\t", collection, &buf);
    alma_function_print(collection, t->neg, &buf);
    tee_alt("\n", collection, &buf);
    LOG( " ckpt 2\n");
    pos_term = alma_function_to_pyobject(collection, t->pos);
    neg_term = alma_function_to_pyobject(collection, t->neg);
    tmp = Py_BuildValue("OO", pos_term, neg_term);
    PyList_Append(resolvent_lst, tmp);
    Py_DECREF(pos_term);
    Py_DECREF(neg_term);
    Py_DECREF(tmp);


    LOG( " ckpt 3\n");
    x = clause_to_pyobject(collection, t->x);
    LOG( " ckpt 3.1\n");
    y = clause_to_pyobject(collection, t->y);
    LOG( " ckpt 3.2\n");
    LOG( "tmp refs %ld \n", Py_REFCNT(tmp));
    tmp = Py_BuildValue("OOd",x, y, data->priority);
    PyList_Append(py_lst, tmp);
    Py_DECREF(x);
    Py_DECREF(y);
    LOG( "tmp refs %ld \n", Py_REFCNT(tmp));
    Py_DECREF(tmp);
    LOG("x refs %ld \n---------------------------------------\n", Py_REFCNT(x));
    LOG("y refs %ld \n---------------------------------------\n", Py_REFCNT(y));
    LOG("tmp refs %ld \n---------------------------------------\n", Py_REFCNT(tmp));



    i = i->next;
    LOG( " ckpt 4\n");
  }


  //clauses_string = malloc(buf.size + 1);
  //clauses_string = PyMem_Malloc(buf.size + 1);

  //strcpy(clauses_string, buf.buffer);
  //clauses_string[buf.size] = '\0';


  result = Py_BuildValue("(O,O, s)",py_lst, resolvent_lst, buf.buffer);
  LOG( " resolvent_lst refs %ld \n", Py_REFCNT(resolvent_lst));
  Py_DECREF(resolvent_lst);
  Py_DECREF(py_lst);
  LOG( " py_lst refs %ld \n================================================================================\n", Py_REFCNT(py_lst));
  LOG( " resolvent_lst refs %ld \n================================================================================\n", Py_REFCNT(resolvent_lst));

  free(buf.buffer);
  return result;
}




static PyObject * alma_kbprint(PyObject *self, PyObject *args) {
  long alma_kb;
  PyObject *result;

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

  result = Py_BuildValue("(s,l)",buf.buffer,buf.size);
  free(buf.buffer);

  return result;
}

static PyObject * alma_halt(PyObject *self, PyObject *args) {
  long alma_kb;

  LOG( "halting\n");
  if (!PyArg_ParseTuple(args, "l",&alma_kb))
    return NULL;

  kb_halt((kb *)alma_kb);

  Py_INCREF(Py_None);
  return Py_None;
}

static PyObject * alma_add(PyObject *self, PyObject *args) {
  const char *input;
  char *assertion;
  int len;
  long alma_kb;
  PyObject *result;

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


  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //fprintf(stderr, "done.\n");
  return result;
}

static PyObject * alma_del(PyObject *self, PyObject *args) {
  const char *input;
  char *assertion;
  int len;
  long alma_kb;
  PyObject *result;

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

  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);
  
  //fprintf(stderr, "done.\n");
  return result;
}

static PyObject * alma_update(PyObject *self, PyObject *args) {
  const char *input;
  char *assertion;
  int len;
  long alma_kb;
  PyObject *result;

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

  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //fprintf(stderr, " done.\n");
  return result;
}

static PyObject * alma_obs(PyObject *self, PyObject *args) {
  const char *input;
  char *assertion;
  int len;
  long alma_kb;
  PyObject *result;

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

  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //fprintf(stderr, " done.\n");
  return result;
}

static PyObject * alma_bs(PyObject *self, PyObject *args) {
  const char *input;
  char *assertion;
  int len;
  long alma_kb;
  PyObject *result;

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

  // TODO:  do we need to append a '\0'?
  result = Py_BuildValue("s",buf.buffer);
  free(buf.buffer);

  //fprintf(stderr, " done.\n");
  return result;
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
  {"single_prb_to_res_task", single_prb_to_resolutions, METH_VARARGS,"Move single pre-resolution task list to resolution task heap."},
  {"res_task_buf", get_res_buf, METH_VARARGS,"Get resolution task heap."},
  {"kbprint", alma_kbprint, METH_VARARGS,"Print out entire alma kb."},
  {"halt", alma_halt, METH_VARARGS,"Stop an alma kb."},
  {"add", alma_add, METH_VARARGS,"Add a clause or formula to an alma kb."},
  {"kbdel", alma_del, METH_VARARGS,"Delete a clause or formula from an alma kb."},
  {"update", alma_update, METH_VARARGS,"Update a clause or formula in an alma kb."},
  {"obs", alma_obs, METH_VARARGS,"Observe something in an alma kb."},
  {"bs", alma_bs, METH_VARARGS,"Backsearch an alma kb."},
  {"set_priorities", alma_set_priorities, METH_VARARGS, "Set subjects and priorities."},
  {"memtest_alloc", alma_memtest_alloc, METH_VARARGS, "For debugging:  allocates memory."},
  {"memtest_free", alma_memtest_free, METH_VARARGS, "For debugging:  allocates memory."},
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
  //int  log_mode;
  int differential_priorities;
  int res_heap_size;

  char *file;
  char *agent;
  //char *trialnum;
  //char *log_dir;

  rl_init_params rip;

  PyObject *list_item;
  PyObject *subject_name_list;
  PyObject *subject_priority_list;
  Py_ssize_t subj_list_len;
  PyObject *result;

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
      LOG( "Got subject name %s\n", PyUnicode_AS_DATA(list_item));
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

    LOG( "subj_list_len %ld\n", subj_list_len);
    LOG( "PyList_Size(subject_priority_list) %ld\n",  PyList_Size(subject_priority_list));
    assert(subj_list_len ==  PyList_Size(subject_priority_list));
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

  rip.tracking_resolutions = 0; // Not really using this right now.
  rip.tracking_subjects = collection_subjects;
  rip.tracking_priorities = collection_priorities;
  rip.use_lists = 1;
  rip.use_res_pre_buffer = 1;

  kb_init(&alma_kb,file,agent,"PyALMA", "/tmp", verbose, differential_priorities, res_heap_size, rip,  &buf, 0);

  result =  Py_BuildValue("(l,s),",(long)alma_kb,buf.buffer);
  free(buf.buffer);

  return  result;
  }


  /*
static PyObject * alma_init(PyObject *self, PyObject *args) {
  int verbose, log_mode;
  PyObject *ret_val;
  char *file;
  char *agent;
  char *trialnum;
  char *log_dir;

  if (!PyArg_ParseTuple(args, "iissss", &verbose, &log_mode, &file, &agent, &trialnum, &log_dir))
    return NULL;
  }
  collection = (kb *)alma_kb;
  collection->prb_threshold = threshold;
  pre_res_buffer_to_heap(collection, 1);
  return Py_None;

  enable_python_mode();

  kb_str buf;
  buf.size = 0;
  buf.limit = 1000;
  buf.buffer = malloc(buf.limit);
  buf.buffer[0] = '\0';

  kb *alma_kb;
  kb_init(&alma_kb,file,agent, trialnum, log_dir, verbose, &buf, log_mode);

  //  ret_val = malloc(buf.size + 1);
  //  strcpy(ret_val,buf.buffer);
  //  ret_val[buf.size] = '\0';

  buf.buffer[buf.size] = '\0';
  ret_val = Py_BuildValue("(l,s),",(long)alma_kb,buf.buffer);

  free(buf.buffer);

  return ret_val;
  } */


/*

// Python2
PyMODINIT_FUNC
initalma(void)
{
  (void) Py_InitModule("alma", AlmaMethods);
}

*/

    /*
     Replicate py_test_halt.py so that we can run through valgrind.
     Can this run by linking to libpython?   Doesn't seem to work as expected...
   */

  /*
  int main(int argc, char **argv) {
  PyObject *alma_inst;
  PyObject *alma_inst_res;
  PyObject *alma_args;
  alma_args = Py_BuildValue("ll", 123, 123);



  alma_args = Py_BuildValue("issii", 1, "glut_control/qlearning3.pl", "0", 1, 1000);

  kb *alma_kb;
  int alma_res_ptr;
  char *alma_ret_val;
  alma_inst_res = alma_init(NULL, alma_args);
  if (!PyArg_ParseTuple(alma_inst_res, "ls", &alma_kb, &alma_ret_val)) {
    fprintf(stderr, "Couln't initialize alma\n");
    exit(1);
  }


  }


  */



