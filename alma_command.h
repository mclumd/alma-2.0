#ifndef alma_command_h
#define alma_command_h

typedef struct kb kb;

void kb_init(kb **collection, char *file);
void kb_step(kb *collection);
void kb_print(kb *collection);
void kb_halt(kb *collection);
void kb_assert(kb *collection, char *string);
void kb_remove(kb *collection, char *string);

#endif
