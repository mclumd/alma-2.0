#include <stdarg.h>
#include "alma_print.h"
#include "alma_clause.h"
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

static void alma_function_print(alma_function *func, kb_logger *logger);

static void alma_term_print(alma_term *term, kb_logger *logger) {
  if (term->type == VARIABLE) {
    tee_alt("%s%lld", logger, term->variable->name, term->variable->id);
  }
  else if (term->type == FUNCTION) {
    alma_function_print(term->function, logger);
  }
  else if (term->type == QUOTE) {
    tee_alt("\"", logger);
    if (term->quote->type == SENTENCE)
      alma_fol_print(term->quote->sentence, logger);
    else
      clause_print(term->quote->clause_quote, logger);
    tee_alt("\"", logger);
  }
  else {
    char *backticks = malloc(term->quasiquote->backtick_count+1);
    for (int i = 0; i < term->quasiquote->backtick_count; i++)
      backticks[i] = '`';
    backticks[term->quasiquote->backtick_count] = '\0';
    tee_alt("%s%s%lld", logger, backticks, term->quasiquote->variable->name, term->quasiquote->variable->id);
    free(backticks);
  }
}

static void alma_function_print(alma_function *func, kb_logger *logger) {
  tee_alt("%s", logger, func->name);
  if (func->term_count > 0) {
    tee_alt("(", logger);
    for (int i = 0; i < func->term_count; i++) {
      if (i > 0)
        tee_alt(", ", logger);
      alma_term_print(func->terms + i, logger);
    }
    tee_alt(")", logger);
  }
}

void alma_fol_print(alma_node *node, kb_logger *logger) {
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

    tee_alt("(", logger);
    if (node->fol->op == NOT)
      tee_alt("%s", logger, op);
    alma_fol_print(node->fol->arg1, logger);

    if (node->fol->arg2 != NULL) {
      tee_alt(" %s ", logger, op);
      alma_fol_print(node->fol->arg2, logger);
    }
    tee_alt(")", logger);
  }
  else
    alma_function_print(node->predicate, logger);
}

static void lits_print(alma_function **lits, int count, char *delimiter, int negate, kb_logger *logger) {
  for (int i = 0; i < count; i++) {
    if (negate)
      tee_alt("~", logger);
    alma_function_print(lits[i], logger);
    if (i < count-1)
      tee_alt(" %s ", logger, delimiter);
  }
}

void clause_print(clause *c, kb_logger *logger) {
  // Print fif in original format
  if (c->tag == FIF) {
    for (int i = 0; i < c->fif->premise_count; i++) {
      alma_function *f = fif_access(c, i);
      if (c->fif->ordering[i] < 0) {
        alma_function_print(f, logger);
      }
      else {
        tee_alt("~", logger);
        alma_function_print(f, logger);
      }
      if (i < c->fif->premise_count-1)
        tee_alt(" /\\", logger);
      tee_alt(" ", logger);
    }
    tee_alt("-f-> ", logger);

    for (int i = 0; i < c->fif->num_conclusions; i++) {
      if (c->fif->conclusions[i]->tag == FIF)
        tee_alt("(", logger);
      clause_print(c->fif->conclusions[i], logger);
      if (c->fif->conclusions[i]->tag == FIF)
        tee_alt(")", logger);
      if (i < c->fif->num_conclusions-1)
        tee_alt(" /\\ ", logger);
    }
  }
  // Non-fif case
  else {
    if (c->pos_count == 0) {
      if (c->tag == BIF) {
        lits_print(c->neg_lits, c->neg_count, "/\\", 0, logger);
        tee_alt(" -b-> F", logger);
      }
      else
        lits_print(c->neg_lits, c->neg_count, "\\/", 1, logger);
    }
    else if (c->neg_count == 0) {
      if (c->tag == BIF)
        tee_alt("T -b-> ", logger);
      lits_print(c->pos_lits, c->pos_count, "\\/", 0, logger);
    }
    else {
      lits_print(c->neg_lits, c->neg_count, "/\\", 0, logger);
      tee_alt(" -%c-> ", logger, c->tag == BIF ? 'b' : '-');
      lits_print(c->pos_lits, c->pos_count, "\\/", 0, logger);
    }
  }

  if (c->parents != NULL || c->children != NULL ||
      c->equiv_bel_up != NULL || c->equiv_bel_down != NULL) {
    tee_alt(" (", logger);
    if (c->parents != NULL) {
      tee_alt("parents: ", logger);
      for (int i = 0; i < c->parent_set_count; i++) {
        tee_alt("[", logger);
        for (int j = 0; j < c->parents[i].count; j++) {
          tee_alt("%ld", logger, c->parents[i].clauses[j]->index);
          if (j < c->parents[i].count-1)
            tee_alt(", ", logger);
        }
        tee_alt("]", logger);
        if (i < c->parent_set_count-1)
          tee_alt(", ", logger);
      }
      if (c->children != NULL ||   c->equiv_bel_up != NULL || c->equiv_bel_down != NULL)
        tee_alt(", ", logger);
    }
    if (c->children != NULL) {
      tee_alt("children: ", logger);
      for (int i = 0; i < c->children_count; i++) {
        tee_alt("%ld", logger, c->children[i]->index);
        if (i < c->children_count-1)
          tee_alt(", ", logger);
      }
      if (c->equiv_bel_up != NULL || c->equiv_bel_down != NULL)
        tee_alt(", ", logger);
    }
    if (c->equiv_bel_up != NULL || c->equiv_bel_down != NULL) {
      tee_alt("equiv: ", logger);
      if (c->equiv_bel_up != NULL && c->equiv_bel_down == NULL) {
        tee_alt("%ld", logger, c->equiv_bel_up->index);
      }
      else if (c->equiv_bel_up == NULL && c->equiv_bel_down != NULL) {
        tee_alt("%ld", logger, c->equiv_bel_down->index);
      }
      else {
        tee_alt("%ld, %ld", logger, c->equiv_bel_up->index, c->equiv_bel_down->index);
      }
    }
    tee_alt(")", logger);

    if (c->paused >= 0) {
      tee_alt(" [pause]", logger);
    }
  }
  c->dirty_bit = 0;
  //tee_alt(" (L%ld)", c->acquired);
}

void print_unify(alma_function *pos, long pos_idx, alma_function *neg, long neg_idx, kb_logger *logger) {
  tee_alt("Unifying ", logger);
  alma_function_print(pos, logger);
  tee_alt(" (%ld) with ", logger, pos_idx);
  alma_function_print(neg, logger);
  tee_alt(" (%ld)\n", logger, neg_idx);
}

void print_matches(var_match_set *v, kb_logger *logger){
  for (int i = 0; i < v->levels; i++) {
    if (v->level_counts[i] != 0) {
      tee_alt("Level %d: ", logger, i);
      for (int j = 0; j < v->level_counts[i]; j++) {
        tee_alt("%lld + %lld", logger, v->matches[i][j].x, v->matches[i][j].y);
        if (j < v->level_counts[i]-1)
          tee_alt(", ", logger);
        else
          tee_alt("\n", logger);
      }
    }
  }
}

void print_bindings(binding_list *theta, int print_all, int last_newline, kb_logger *logger) {
  if (theta->num_bindings == 0) {
    tee_alt("Empty binding set\n", logger);
  }
  else {
    for (int i = 0; i < theta->num_bindings; i++) {
      if (theta->list[i].quasi_quote_level > 0) {
        char *backticks = malloc(theta->list[i].quasi_quote_level+1);
        for (int j = 0; j < theta->list[i].quasi_quote_level; j++)
          backticks[j] = '`';
        backticks[theta->list[i].quasi_quote_level] = '\0';
        tee_alt("%s", logger, backticks);
        free(backticks);
      }
      tee_alt("%s%lld", logger, theta->list[i].var->name, theta->list[i].var->id);
      tee_alt(" / ", logger);
      alma_term_print(theta->list[i].term, logger);
      tee_alt(" (%d quote)", logger, theta->list[i].quote_level);

      if (i < theta->num_bindings-1) {
        if (print_all)
          tee_alt("\n", logger);
        else
          tee_alt(", ", logger);
      }
    }
    if (print_all) {
      if (theta->num_bindings > 0)
        tee_alt("\n", logger);
      print_matches(theta->quoted_var_matches, logger);
    }
  }
  if (last_newline)
    tee_alt("\n", logger);
}

void tee_alt(char const *content, ...) {
  va_list ap, copy_ap;
  //char fake_str[1000];
  kb_logger *logger;
  int bytes = 0, size = 0;
  va_start(ap, content);

  logger = va_arg(ap, kb_logger *);
  if (python_mode && logger != NULL) {
    //va_copy(copy_ap, ap);
    size = logger->buf->limit - logger->buf->size;
    //bytes = vsnprintf(fake_str, size, content, copy_ap);
    va_copy(copy_ap, ap);
    bytes = vsnprintf(&(logger->buf->buffer[logger->buf->size]), size, content, copy_ap);
    //printf("TEEEEEE: %s, %d --- %d\n", fake_str, size, bytes);

    while (bytes >= size) {
      //printf("IN BYTES LOOP\n");
      logger->buf->limit += 1000;
      logger->buf->buffer = realloc(logger->buf->buffer, logger->buf->limit);
      va_copy(copy_ap,ap);
      size = logger->buf->limit - logger->buf->size;
      bytes = vsnprintf(&(logger->buf->buffer[logger->buf->size]), size, content, copy_ap);
    }
    logger->buf->size += bytes;
    va_end(copy_ap);
  } else {
    vprintf(content, ap);
  }
  va_end(ap);

  if (logs_on) {
    va_start(ap, content);
    logger = va_arg(ap, kb_logger *);
    vfprintf(logger->log, content, ap);
    va_end(ap);
  }
}
