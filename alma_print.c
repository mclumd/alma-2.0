#include <stdarg.h>
#include "alma_print.h"
#include "alma_formula.h"
#include "alma_fif.h"


char logs_on;
char python_mode;

void enable_logs() {
  logs_on = 1;
}

void enable_python_mode() {
  python_mode = 1;
}

void disable_python_mode() {
  python_mode = 0;
}

static void alma_function_print(kb *collection, alma_function *func, kb_str *buf);

static void alma_term_print(kb *collection, alma_term *term, kb_str *buf) {
  if (term->type == VARIABLE) {
    tee_alt("%s%lld", collection, buf, term->variable->name, term->variable->id);
  }
  else if (term->type == FUNCTION) {
    alma_function_print(collection, term->function, buf);
  }
  else if (term->type == QUOTE) {
    tee_alt("\"", collection, buf);
    if (term->quote->type == SENTENCE)
      alma_fol_print(collection, term->quote->sentence, buf);
    else
      clause_print(collection, term->quote->clause_quote, buf);
    tee_alt("\"", collection, buf);
  }
  else {
    char *backticks = malloc(term->quasiquote->backtick_count+1);
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      backticks[i] = '`';
    backticks[term->quasiquote->backtick_count] = '\0';
    tee_alt("%s%s%lld", collection, buf, backticks, term->quasiquote->variable->name, term->quasiquote->variable->id);
    free(backticks);
  }
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

    for (int i = 0; i < c->fif->num_conclusions; i++) {
      if (c->fif->conclusions[i]->tag == FIF)
        tee_alt("(", collection, buf);
      clause_print(collection, c->fif->conclusions[i], buf);
      if (c->fif->conclusions[i]->tag == FIF)
        tee_alt(")", collection, buf);
      if (i < c->fif->num_conclusions-1)
        tee_alt(" /\\ ", collection, buf);
    }
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
        tee_alt("%ld", collection, buf, c->children[i]->index);
        if (i < c->children_count-1)
          tee_alt(", ", collection, buf);
      }
    }
    tee_alt(")", collection, buf);
  }
  c->dirty_bit = 0;
  //tee_alt(" (L%ld)", c->acquired);
}

void print_unify(kb *collection, alma_function *pos, long pos_idx, alma_function *neg, long neg_idx, kb_str *buf) {
  tee_alt("Unifying ", collection, buf);
  alma_function_print(collection, pos, buf);
  tee_alt(" (%ld) with ", collection, buf, pos_idx);
  alma_function_print(collection, neg, buf);
  tee_alt(" (%ld)\n", collection, buf, neg_idx);
}

void print_matches(kb *collection, var_match_set *v, kb_str *buf){
  for (int i = 0; i < v->levels; i++) {
    if (v->level_counts[i] != 0) {
      tee_alt("Level %d: ", collection, buf, i);
      for (int j = 0; j < v->level_counts[i]; j++) {
        tee_alt("%lld + %lld", collection, buf, v->matches[i][j].x, v->matches[i][j].y);
        if (j < v->level_counts[i]-1)
          tee_alt(", ", collection, buf);
        else
          tee_alt("\n", collection, buf);
      }
    }
  }
}

void print_bindings(kb *collection, binding_list *theta, int print_all, int last_newline, kb_str *buf) {
  for (int i = 0; i < theta->num_bindings; i++) {
    if (theta->list[i].quasi_quote_level > 0) {
      char *backticks = malloc(theta->list[i].quasi_quote_level+1);
      for (int j = 0; j < theta->list[i].quasi_quote_level; j++)
        backticks[j] = '`';
      backticks[theta->list[i].quasi_quote_level] = '\0';
      tee_alt("%s", collection, buf, backticks);
      free(backticks);
    }
    tee_alt("%s%lld", collection, buf, theta->list[i].var->name, theta->list[i].var->id);
    tee_alt(" / ", collection, buf);
    alma_term_print(collection, theta->list[i].term, buf);
    tee_alt(" (%d quote)", collection, buf, theta->list[i].quote_level);

    if (i < theta->num_bindings-1) {
      if (print_all)
        tee_alt("\n", collection, buf);
      else
        tee_alt(", ", collection, buf);
    }
  }
  if (print_all) {
    if (theta->num_bindings > 0)
      tee_alt("\n", collection, buf);
    print_matches(collection, theta->quoted_var_matches, buf);
  }
  if (last_newline)
    tee_alt("\n", collection, buf);
}

void tee_alt(char const *content, ...) {
  va_list ap, copy_ap;
  //  char fake_str[1000];
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
