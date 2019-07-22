#include "alma_print.h"
#include "alma_formula.h"

static void alma_function_print(alma_function *func);

static void alma_term_print(alma_term *term) {
  switch (term->type) {
    case VARIABLE:
      printf("%s%lld", term->variable->name, term->variable->id);
      break;
    case CONSTANT:
      printf("%s", term->constant->name);
      break;
    case FUNCTION:
      alma_function_print(term->function);
      break;
  }
}

static void alma_function_print(alma_function *func) {
  printf("%s", func->name);
  if (func->term_count > 0) {
    printf("(");
    for (int i = 0; i < func->term_count; i++) {
      if (i > 0)
        printf(", ");
      alma_term_print(func->terms + i);
    }
    printf(")");
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
      default:
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
      printf("%sFOL: %s, tag: %s\n", spacing, op, tag);
    }
    else {
      printf("%sFOL: %s\n", spacing, op);
    }

    alma_fol_print_rec(node->fol->arg1, indent+1);
    if (node->fol->arg2 != NULL) {
      alma_fol_print_rec(node->fol->arg2, indent+1);
    }
  }
  else {
    printf("%sPREDICATE: ", spacing);
    alma_function_print(node->predicate);
    printf("\n");
  }
  free(spacing);
}

void alma_fol_print(alma_node *node) {
  alma_fol_print_rec(node, 0);
}

static void lits_print(alma_function **lits, int count, char *delimiter, int negate) {
  for (int i = 0; i < count; i++) {
    if (negate)
      printf("not(");
    alma_function_print(lits[i]);
    if (negate)
      printf(")");
    if (i < count-1)
      printf(" %s ", delimiter);
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
        printf("not(");
        alma_function_print(f);
        printf(")");
      }
      if (i < c->fif->premise_count-1)
        printf(" /\\");
      printf(" ");
    }
    printf("-f-> ");
    alma_function_print(c->fif->conclusion);
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        lits_print(c->neg_lits, c->neg_count, "/\\", 0);
        printf(" -b-> F");
      }
      else
        lits_print(c->neg_lits, c->neg_count, "\\/", 1);
    }
    else if (c->neg_count == 0) {
      if (c->tag == BIF)
        printf("T -b-> ");
      lits_print(c->pos_lits, c->pos_count, "\\/", 0);
    }
    else {
      lits_print(c->neg_lits, c->neg_count, "/\\", 0);
      printf(" -%c-> ", c->tag == BIF ? 'b' : '-');
      lits_print(c->pos_lits, c->pos_count, "\\/", 0);
    }
    if (c->parents != NULL || c->children != NULL) {
      printf(" (");
      if (c->parents != NULL) {
        printf("parents: ");
        for (int i = 0; i < c->parent_set_count; i++) {
          printf("[");
          for (int j = 0; j < c->parents[i].count; j++) {
            printf("%ld", c->parents[i].clauses[j]->index);
            if (j < c->parents[i].count-1)
              printf(", ");
          }
          printf("]");
          if (i < c->parent_set_count-1)
            printf(", ");
        }
        if (c->children != NULL)
          printf(", ");
      }
      if (c->children != NULL) {
        printf("children: ");
        for (int i = 0; i < c->children_count; i++) {
          printf("%ld",c->children[i]->index);
          if (i < c->children_count-1)
            printf(", ");
        }
      }
      printf(")");
    }
  }
  //printf(" (L%ld)", c->learned);
}

void print_bindings(binding_list *theta) {
  for (int i = 0; i < theta->num_bindings; i++) {
    printf("%s%lld/", theta->list[i].var->name, theta->list[i].var->id);
    alma_term_print(theta->list[i].term);
    if (i < theta->num_bindings-1)
      printf(", ");
  }
}
