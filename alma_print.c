#include "alma_print.h"
#include "alma_formula.h"
#include "alma_fif.h"
static void alma_function_print(alma_function *func);

void res_task_print(res_task *t) {
  clause_print(t->x);
  tee("\t");
  clause_print(t->y);
  tee("\t");
  alma_function_print(t->pos);
  tee("\t");
  alma_function_print(t->neg);
}



static void alma_term_print(alma_term *term) {
  if (term->type == VARIABLE)
    tee("%s%lld", term->variable->name, term->variable->id);
  else
    alma_function_print(term->function);
}

static void alma_function_print(alma_function *func) {
  tee("%s", func->name);
  if (func->term_count > 0) {
    tee("(");
    for (int i = 0; i < func->term_count; i++) {
      if (i > 0)
        tee(", ");
      alma_term_print(func->terms + i);
    }
    tee(")");
  }
}

static void alma_fol_print_rec(alma_node *node, int indent) {
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
      tee("%sFOL: %s, tag: %s\n", spacing, op, tag);
    }
    else {
      tee("%sFOL: %s\n", spacing, op);
    }

    alma_fol_print_rec(node->fol->arg1, indent+1);
    if (node->fol->arg2 != NULL) {
      alma_fol_print_rec(node->fol->arg2, indent+1);
    }
  }
  else {
    tee("%sPREDICATE: ", spacing);
    alma_function_print(node->predicate);
    tee("\n");
  }
  free(spacing);
}

void alma_fol_print(alma_node *node) {
  alma_fol_print_rec(node, 0);
}

static void lits_print(alma_function **lits, int count, char *delimiter, int negate) {
  for (int i = 0; i < count; i++) {
    if (negate)
      tee("~");
    alma_function_print(lits[i]);
    if (i < count-1)
      tee(" %s ", delimiter);
  }
}

void clause_print(clause *c) {
  // Print fif in original format
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        alma_function_print(f);
      }
      else {
        tee("~");
        alma_function_print(f);
      }
      if (i < c->fif->premise_count-1)
        tee(" /\\");
      tee(" ");
    }
    tee("-f-> ");
    if (c->fif->neg_conc)
      tee("~");
    alma_function_print(c->fif->conclusion);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        lits_print(c->neg_lits, c->neg_count, "/\\", 0);
        tee(" -b-> F");
      }
      else
        lits_print(c->neg_lits, c->neg_count, "\\/", 1);
    }
    else if (c->neg_count == 0) {
      if (c->tag == BIF)
        tee("T -b-> ");
      lits_print(c->pos_lits, c->pos_count, "\\/", 0);
    }
    else {
      lits_print(c->neg_lits, c->neg_count, "/\\", 0);
      tee(" -%c-> ", c->tag == BIF ? 'b' : '-');
      lits_print(c->pos_lits, c->pos_count, "\\/", 0);
    }
  }
  if (c->parents != NULL || c->children != NULL) {
    tee(" (");
    if (c->parents != NULL) {
      tee("parents: ");
      for (int i = 0; i < c->parent_set_count; i++) {
        tee("[");
        for (int j = 0; j < c->parents[i].count; j++) {
          tee("%ld", c->parents[i].clauses[j]->index);
          if (j < c->parents[i].count-1)
            tee(", ");
        }
        tee("]");
        if (i < c->parent_set_count-1)
          tee(", ");
      }
      if (c->children != NULL)
        tee(", ");
    }
    if (c->children != NULL) {
      tee("children: ");
      for (int i = 0; i < c->children_count; i++) {
        tee("%ld",c->children[i]->index);
        if (i < c->children_count-1)
          tee(", ");
      }
    }
    tee(")");
  }
  //tee(" (L%ld)", c->learned);
}

void print_bindings(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++) {
    tee("%s%lld/", theta->list[i].var->name, theta->list[i].var->id);
    alma_term_print(theta->list[i].term);
    if (i < theta->num_bindings-1)
      tee(", ");
  }
}

void tee(char const *content, ...) {
  va_list ap;
  va_start(ap, content);
  vprintf(content, ap);
  va_end(ap);
  va_start(ap, content);
  vfprintf(almalog, content, ap);
  va_end(ap);
}
