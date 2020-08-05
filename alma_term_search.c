#include "alma_term_search.h"
#include <stdlib.h>
#include <string.h>

int alma_term_search(alma_term *term, char *search_term) {
  if (term->type == VARIABLE)
    return strstr(term->variable->name, search_term) == NULL ? 0 : 1;
  else
    return alma_function_search(term->function, search_term);
}

int alma_function_search(alma_function *func, char *search_term) {
  if (strstr(func->name, search_term) != NULL) return 1;
  if (func->term_count > 0) {
      for (int i = 0; i < func->term_count; i++) {
          if (alma_term_search(func->terms + i, search_term)) return 1;
        }
    }
  return 0;
}

int lits_search(alma_function **lits, int count, char *search_term) {
  for (int i = 0; i < count; i++) {
    if (alma_function_search(lits[i], search_term)) return 1;
  }
  return 0;
}


/*
string_set alma_term_constants(alma_term *term) {
  if (term->type == VARIABLE)
    return new_string_set(term->variable->name);
  else
    return alma_function_constants(term->function);
}

string_set alma_function_constants(alma_function **lits) {
  string_set result;
  result = empty_string_set;
  if (func->term_count > 0) {
    for (int i = 0; i < func->term_count; i++) {
      result = string_set_union(result, alma_term_constants(func->terms + i));
    }
  }
  return result;
}
  
string_set alma_lits_constants(alma_function **lits) {
  string_set result;
  result = empty_string_set;
  for (int i = 0; i < count; i++) {
    result = string_set_union(result, alma_function_constants(lits[i]));
  }
  return result;
}
*/
