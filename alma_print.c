#include <stdarg.h>
#include "alma_print.h"
#include "alma_formula.h"
#include "alma_fif.h"


char logs_on;
char python_mode;

static void alma_function_print(kb *collection, alma_function *func, kb_str *buf);
static void alma_quote_print(kb *collection, alma_quote *quote, kb_str *buf);


static void alma_function_print(kb *collection, alma_function *func, kb_str *buf);

void enable_logs() {
  logs_on = 1;
}

/*static void alma_function_print(alma_function *func, kb_str *buf);
static void alma_function_print(alma_function *func, kb_str *buf );
static void alma_quote_print(alma_quote *quote, kb_str *buf);
*/

void res_task_print(kb *collection, res_task *t, kb_str *buf) {
  clause_print(collection, t->x, buf);
  tee_alt("\t", buf);
  clause_print(collection, t->y, buf);
  tee_alt("\t", buf);
  alma_function_print(collection, t->pos, buf);
  tee_alt("\t", buf);
  alma_function_print(collection, t->neg, buf);
}



void enable_python_mode() {
  python_mode = 1;
}

void disable_python_mode() {
  python_mode = 0;
}

static void alma_term_print(kb *collection, alma_term *term, kb_str *buf) {
  if (term->type == VARIABLE)
    tee_alt("%s%lld", collection, buf, term->variable->name, term->variable->id);
  else if (term->type == FUNCTION)
    alma_function_print(collection, term->function, buf);
  else
    alma_quote_print(collection, term->quote, buf);
}

static void alma_function_print(kb *collection, alma_function *func, kb_str *buf) {
  tee_alt("%s", collection, buf, func->name);
  if (func->term_count > 0) {
    tee_alt("(", collection, buf);
    for (int i = 0; i < func->term_count; i++) {
      if (i > 0)
        tee_alt(", ", collection, buf);
      alma_term_print(collection, func->terms + i, buf);
    }
    tee_alt(")", collection, buf);
  }
}

static void alma_quote_print(kb *collection, alma_quote *quote, kb_str *buf) {
  tee_alt("\"", collection, buf);
  if (quote->type == SENTENCE)
    alma_fol_print(collection, quote->sentence, buf);
  else
    clause_print(collection, quote->clause_quote, buf);
  tee_alt("\"", collection, buf);
}

void alma_fol_print(kb *collection, alma_node *node, kb_str *buf) {
  if (node->type == FOL) {
    char *op;
    switch (node->fol->op) {
      case NOT:
        op = "~"; break;
      case OR:
        op = "\\/"; break;
      case AND:
        op = "/\\"; break;
      case IF:
        if (node->fol->tag == FIF)
          op = "-f->";
        else if (node->fol->tag == BIF)
          op = "-b->";
        else
          op = "--->";
        break;
    }

    tee_alt("(", collection, buf);
    if (node->fol->op == NOT)
      tee_alt("%s", collection, buf, op);
    alma_fol_print(collection, node->fol->arg1, buf);

    if (node->fol->arg2 != NULL) {
      tee_alt(" %s ", collection, buf, op);
      alma_fol_print(collection, node->fol->arg2, buf);
    }
    tee_alt(")", collection, buf);
  }
  else
    alma_function_print(collection, node->predicate, buf);
}

static void lits_print(kb *collection, alma_function **lits, int count, char *delimiter, int negate, kb_str *buf) {
  for (int i = 0; i < count; i++) {
    if (negate)
      tee_alt("~", collection, buf);
    alma_function_print(collection, lits[i], buf);
    if (i < count-1)
      tee_alt(" %s ", collection, buf, delimiter);
  }
}

void clause_print(kb *collection, clause *c, kb_str *buf) {
  // Print fif in original format
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        alma_function_print(collection, f, buf);
      }
      else {
        tee_alt("~", collection, buf);
        alma_function_print(collection, f, buf);
      }
      if (i < c->fif->premise_count-1)
        tee_alt(" /\\", collection, buf);
      tee_alt(" ", collection, buf);
    }
    tee_alt("-f-> ", collection, buf);
    if (c->fif->neg_conc)
      tee_alt("~", collection, buf);
    alma_function_print(collection, c->fif->conclusion, buf);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        lits_print(collection, c->neg_lits, c->neg_count, "/\\", 0, buf);
        tee_alt(" -b-> F", collection, buf);
      }
      else
        lits_print(collection, c->neg_lits, c->neg_count, "\\/", 1, buf);
    }
    else if (c->neg_count == 0) {
      if (c->tag == BIF)
        tee_alt("T -b-> ", collection, buf);
      lits_print(collection, c->pos_lits, c->pos_count, "\\/", 0, buf);
    }
    else {
      lits_print(collection, c->neg_lits, c->neg_count, "/\\", 0, buf);
      tee_alt(" -%c-> ", collection, buf, c->tag == BIF ? 'b' : '-');
      lits_print(collection, c->pos_lits, c->pos_count, "\\/", 0, buf);
    }
  }
  if (c->parents != NULL || c->children != NULL) {
    tee_alt(" (", collection, buf);
    if (c->parents != NULL) {
      tee_alt("parents: ", collection, buf);
      for (int i = 0; i < c->parent_set_count; i++) {
        tee_alt("[", collection, buf);
        for (int j = 0; j < c->parents[i].count; j++) {
          tee_alt("%ld", collection, buf, c->parents[i].clauses[j]->index);
          if (j < c->parents[i].count-1)
            tee_alt(", ", collection, buf);
        }
        tee_alt("]", collection, buf);
        if (i < c->parent_set_count-1)
          tee_alt(", ", collection, buf);
      }
      if (c->children != NULL)
        tee_alt(", ", collection, buf);
    }
    if (c->children != NULL) {
      tee_alt("children: ", collection, buf);
      for (int i = 0; i < c->children_count; i++) {
        tee_alt("%ld",collection, buf, c->children[i]->index);
        if (i < c->children_count-1)
          tee_alt(", ", collection, buf);
      }
    }
    tee_alt(")", collection, buf);
  }
  c->dirty_bit = (char) 0;
  //tee_alt(" (L%ld)", c->learned);
}

void print_bindings(kb *collection, binding_list *theta, kb_str *buf) {
  for (int i = 0; i < theta->num_bindings; i++) {
    tee_alt("%s%lld/", collection, buf, theta->list[i].var->name, theta->list[i].var->id);
    alma_term_print(collection, theta->list[i].term, buf);
    if (i < theta->num_bindings-1)
      tee_alt(", ", collection, buf);
  }
}

void tee_alt(char const *content, ...) {
  va_list ap, copy_ap;
  kb_str *dest;
  kb *collection;
  int bytes = 0, size = 0;
  va_start(ap, content);

  collection = va_arg(ap, kb *);
  dest = va_arg(ap, kb_str *);
  if (python_mode) {
    //    va_copy(copy_ap, ap);
    size = dest->limit - dest->size;
    //    bytes = vsnprintf(fake_str, size, content, copy_ap);
    va_copy(copy_ap, ap);
    bytes = vsnprintf(&(dest->buffer[dest->size]), size, content, copy_ap);
    //    printf("TEEEEEE: %s, %d --- %d\n", fake_str, size, bytes);

    while (bytes >= size) {
      //      printf("IN BYTES LOOP\n");
      dest->limit += 1000;
      dest->buffer = realloc(dest->buffer, dest->limit);
      va_copy(copy_ap,ap);
      size = dest->limit - dest->size;
      bytes = vsnprintf(&(dest->buffer[dest->size]), size, content, copy_ap);
    }
    dest->size += bytes;
    va_end(copy_ap);
  } else {
    vprintf(content, ap);
  }
  va_end(ap);

  if (logs_on) {
    va_start(ap, content);
    collection = va_arg(ap, kb *);
    dest = va_arg(ap, kb_str *);
    vfprintf(collection->almalog, content, ap);
    va_end(ap);
  }
}

/*
int tee(char const *content, ...) {
  va_list ap, copy_ap;
  int bytes = 0, size = 0;
  kb_str *dest;
  //  char *s = malloc(100);
  //  long test;
  //  va_start(ap, content);
  //  snprintf(s, 100, "blah");
  //  test = va_arg(ap, long);
  //  strcpy(dest->buffer,"ahhh");
  //  dest->size = 4;
  //  va_end(ap);
  return 37;


  if (!python_mode) {
    vprintf(content, ap);
  } else {
    va_copy(copy_ap,ap);
    size = dest->limit - dest->size;
    bytes = vsnprintf(&(dest->buffer[dest->size]), size, content, copy_ap);

    while (bytes == size) {
      dest->limit += 1000;
      dest->buffer = realloc(dest->buffer, dest->limit);
      va_copy(copy_ap,ap);
      size = dest->limit - dest->size;
      bytes = vsnprintf(&(dest->buffer[dest->size]), size, content, copy_ap);
    }
    dest->size += bytes;

    va_end(copy_ap);
  }

  va_end(ap);

  if (logs_on) {
    va_start(ap, content);
    vfprintf(almalog, content, ap);
    va_end(ap);
  }

  return 12;
}
*/
