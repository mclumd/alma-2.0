#ifndef alma_term_search_h
#define alma_term_search_h

#include "alma_kb.h"
#include "alma_backsearch.h"
#include "alma_fif.h"
#include "alma_proc.h"

int lits_search(alma_function **lits, int count, char *search_term);
int alma_term_search(alma_term *term, char *search_term);
int alma_function_search(alma_function *func, char *search_term);

#endif
