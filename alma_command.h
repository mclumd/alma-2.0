#ifndef alma_command_h
#define alma_command_h

#include "alma.h"
#include "alma_kb.h"

void alma_init(alma *reasoner, char **files, int file_count, char *agent, char *trialnum, char *log_dir, int verbose, kb_str *buf, int logon);
void alma_step(alma *reasoner, kb_str *buf);
void alma_print(alma *reasoner, kb_str *buf);
void alma_halt(alma *reasoner);
void alma_assert(alma *reasoner, char *string, kb_str *buf);
void alma_remove(alma *reasoner, char *string, kb_str *buf);
void alma_update(alma *reasoner, char *string, kb_str *buf);
void alma_observe(alma *reasoner, char *string, kb_str *buf);
void alma_backsearch(alma *reasoner, char *string, kb_str *buf);

#endif
