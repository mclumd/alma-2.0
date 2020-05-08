#include "alma_print.h"
#include "alma_formula.h"
#include "alma_fif.h"

char logs_on;
char python_mode;

static void alma_function_print(alma_function *func, kb_str *buf);

void enable_python_mode() {
  python_mode = 1;
}

void disable_python_mode() {
  python_mode = 0;
}

static void alma_term_print(alma_term *term, kb_str *buf) {
  if (term->type == VARIABLE)
    tee_alt("%s%lld", buf, term->variable->name, term->variable->id);
  else
    alma_function_print(term->function, buf);
}

static void alma_function_print(alma_function *func, kb_str *buf) {
  tee_alt("%s", buf, func->name);
  if (func->term_count > 0) {
    tee_alt("(", buf);
    for (int i = 0; i < func->term_count; i++) {
      if (i > 0)
        tee_alt(", ", buf);
      alma_term_print(func->terms + i, buf);
    }
    tee_alt(")", buf);
  }
}

static void alma_fol_print_rec(alma_node *node, int indent, kb_str *buf) {
  char *spacing = malloc(indent*2 + 1);
  if (indent > 0)
    memset(spacing, ' ', indent*2);
  spacing[indent*2] = '\0';

  if (node->type == FOL) {
    char *op;
    switch (node->fol->op) {
      case NOT:
        op = "NOT"; break;
      case OR:
        op = "OR"; break;
      case AND:
        op = "AND"; break;
      case IF:
        op = "IF"; break;
    }

    if (node->fol->tag != NONE) {
      char *tag = "";
      switch (node->fol->tag) {
        case FIF:
          tag = "FIF"; break;
        case BIF:
          tag = "BIF"; break;
        case NONE:
          tag = "NONE"; break;
      }
      tee_alt("%sFOL: %s, tag: %s\n", buf, spacing, op, tag);
    }
    else {
      tee_alt("%sFOL: %s\n", buf, spacing, op);
    }

    alma_fol_print_rec(node->fol->arg1, indent+1, buf);
    if (node->fol->arg2 != NULL) {
      alma_fol_print_rec(node->fol->arg2, indent+1, buf);
    }
  }
  else {
    tee_alt("%sPREDICATE: ", buf, spacing);
    alma_function_print(node->predicate, buf);
    tee_alt("\n", buf);
  }
  free(spacing);
}

void alma_fol_print(alma_node *node, kb_str *buf) {
  alma_fol_print_rec(node, 0, buf);
}

static void lits_print(alma_function **lits, int count, char *delimiter, int negate, kb_str *buf) {
  for (int i = 0; i < count; i++) {
    if (negate)
      tee_alt("~", buf);
    alma_function_print(lits[i], buf);
    if (i < count-1)
      tee_alt(" %s ", buf, delimiter);
  }
}

void clause_print(clause *c, kb_str *buf) {
  // Print fif in original format
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        alma_function_print(f, buf);
      }
      else {
        tee_alt("~", buf);
        alma_function_print(f, buf);
      }
      if (i < c->fif->premise_count-1)
        tee_alt(" /\\", buf);
      tee_alt(" ", buf);
    }
    tee_alt("-f-> ", buf);
    if (c->fif->neg_conc)
      tee_alt("~", buf);
    alma_function_print(c->fif->conclusion, buf);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        lits_print(c->neg_lits, c->neg_count, "/\\", 0, buf);
        tee_alt(" -b-> F", buf);
      }
      else
        lits_print(c->neg_lits, c->neg_count, "\\/", 1, buf);
    }
    else if (c->neg_count == 0) {
      if (c->tag == BIF)
        tee_alt("T -b-> ", buf);
      lits_print(c->pos_lits, c->pos_count, "\\/", 0, buf);
    }
    else {
      lits_print(c->neg_lits, c->neg_count, "/\\", 0, buf);
      tee_alt(" -%c-> ", buf, c->tag == BIF ? 'b' : '-');
      lits_print(c->pos_lits, c->pos_count, "\\/", 0, buf);
    }
  }
  if (c->parents != NULL || c->children != NULL) {
    tee_alt(" (", buf);
    if (c->parents != NULL) {
      tee_alt("parents: ", buf);
      for (int i = 0; i < c->parent_set_count; i++) {
        tee_alt("[", buf);
        for (int j = 0; j < c->parents[i].count; j++) {
          tee_alt("%ld", buf, c->parents[i].clauses[j]->index);
          if (j < c->parents[i].count-1)
            tee_alt(", ", buf);
        }
        tee_alt("]", buf);
        if (i < c->parent_set_count-1)
          tee_alt(", ", buf);
      }
      if (c->children != NULL)
        tee_alt(", ", buf);
    }
    if (c->children != NULL) {
      tee_alt("children: ", buf);
      for (int i = 0; i < c->children_count; i++) {
        tee_alt("%ld",buf, c->children[i]->index);
        if (i < c->children_count-1)
          tee_alt(", ", buf);
      }
    }
    tee_alt(")", buf);
  }
  //tee_alt(" (L%ld)", c->learned);
}

void print_bindings(binding_list *theta, kb_str *buf) {
  for (int i = 0; i < theta->num_bindings; i++) {
    tee_alt("%s%lld/", buf, theta->list[i].var->name, theta->list[i].var->id);
    alma_term_print(theta->list[i].term, buf);
    if (i < theta->num_bindings-1)
      tee_alt(", ", buf);
  }
}

void tee_alt(char const *content, ...) {
  va_list ap, copy_ap;
  kb_str *dest;
  int bytes = 0, size = 0;
  va_start(ap, content);
  dest = va_arg(ap, kb_str *);
  if (python_mode) {
    va_copy(copy_ap, ap);
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
  } else {
    vprintf(content, ap);
  }
  va_end(ap);

  if (logs_on) {
    va_start(ap, content);
    dest = va_arg(ap, kb_str *);
    vfprintf(almalog, content, ap);
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
