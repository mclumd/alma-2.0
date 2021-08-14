CC = gcc
#CFLAGS = -std=c11 -pedantic-errors -Wall -Werror -Wshadow -Wpedantic -g
CFLAGS = -std=gnu11 -pedantic-errors -Wall -Wshadow -Wpedantic -g -mno-avx2 -mtune=generic -mno-avx2 -fPIC -O0

TOMMY = tommyds/tommyds/

all: alma.x shared static python

alma.x: res_task_heap.c alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o res_task_heap.o alma_term_search.o compute_priority.o
	$(CC) alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o res_task_heap.o alma_term_search.o compute_priority.o -o alma.x

alma.o: alma.c alma_command.h alma_kb.h alma_print.h
	$(CC) $(CFLAGS) -c alma.c

tommyhashlin.o: $(TOMMY)tommyhash.c $(TOMMY)tommyhashlin.c $(TOMMY)tommytypes.h $(TOMMY)tommyhash.h $(TOMMY)tommylist.h $(TOMMY)tommyhashlin.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommyhashlin.c

tommyarray.o: $(TOMMY)tommyarray.c $(TOMMY)tommytypes.h $(TOMMY)tommyarray.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommyarray.c

tommylist.o: $(TOMMY)tommylist.c $(TOMMY)tommytypes.h $(TOMMY)tommychain.h $(TOMMY)tommylist.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommylist.c

tommyhash.o: $(TOMMY)tommyhash.c $(TOMMY)tommytypes.h $(TOMMY)tommyhash.h
	$(CC) $(CFLAGS) -c $(TOMMY)tommyhash.c

tommy: $(TOMMY)tommytypes.h $(TOMMY)tommyhash.h $(TOMMY)tommylist.h $(TOMMY)tommyhashlin.h $(TOMMY)tommyarray.h $(TOMMY)tommychain.h
	touch tommy.h

mpc.o: mpc/mpc.c mpc/mpc.h
	$(CC) $(CFLAGS) -c mpc/mpc.c

alma_command.o: alma_command.c alma_command.h alma_kb.h alma_formula.h alma_backsearch.h alma_fif.h alma_parser.h alma_print.h tommy.h
	$(CC) $(CFLAGS) -c alma_command.c

alma_parser.o: alma_parser.c mpc/mpc.h alma_parser.h
	$(CC) $(CFLAGS) -c alma_parser.c

alma_formula.o: alma_formula.c mpc/mpc.h alma_parser.h alma_print.h alma_formula.h
	$(CC) $(CFLAGS) -c alma_formula.c

alma_kb.o: alma_kb.c alma_unify.h alma_formula.h alma_print.h alma_kb.h alma_proc.h alma_backsearch.h alma_fif.h tommy.h
	$(CC) $(CFLAGS) -c alma_kb.c

alma_unify.o: alma_unify.c alma_unify.h alma_formula.h alma_kb.h
	$(CC) $(CFLAGS) -c alma_unify.c

alma_print.o: alma_print.c alma_kb.h alma_formula.h alma_unify.h alma_fif.h alma_print.h
	$(CC) $(CFLAGS) -c alma_print.c

alma_proc.o: alma_proc.c alma_kb.h alma_formula.h alma_unify.h alma_proc.h tommy.h
	$(CC) $(CFLAGS) -c alma_proc.c

alma_fif.o: alma_fif.c alma_kb.h alma_formula.h alma_proc.h alma_fif.h tommy.h
	$(CC) $(CFLAGS) -c alma_fif.c

alma_backsearch.o: alma_backsearch.c alma_kb.h alma_formula.h alma_backsearch.h tommy.h
	$(CC) $(CFLAGS) -c alma_backsearch.c

res_task_heap.o:  res_task_heap.c res_task_heap.h alma_kb.h
	$(CC) $(CFLAGS) -c res_task_heap.c #index_heap.c

res_task_heap.c:  res_task_heap.g alma_kb.h index_heap.h
	$(CC) -std=gnu99 -E -P -DHEADER -Dheap_type=res_task_pri  -Dheap_name=res_task_heap - < "res_task_heap.g" > "res_task_heap.h"
	$(CC) -std=gnu99 -E -P -DSOURCE -Dheap_type=res_task_pri -Dheap_name=res_task_heap -DHEADER_NAME=res_task_heap.h - < "res_task_heap.g" > "res_task_heap.c"
	#$(CC) -std=c99 -E -P -Dheap_name=index_heap -DSOURCE -Dheap_type=index_heap  -DHEADER_NAME=index_heap.h -  < "res_task_heap.g" > "index_heap.c"

alma_term_search.o: alma_term_search.c alma_term_search.h
	$(CC) $(CFLAGS) -c alma_term_search.c

compute_priority.o: compute_priority.c
	$(CC) $(CFLAGS) -c compute_priority.c


clean:
	rm -f *.x *.o *.so build/lib.linux-x86_64-2.7/alma.so

shared: alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o compute_priority.o res_task_heap.o alma_term_search.o
	$(CC) -shared -o libalma.so tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o compute_priority.o res_task_heap.o alma_term_search.o alma.o
	cp libalma.so ~/.local/lib/libalma.so

python:  almamodule.c
	python setup.py install --user

static: alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o compute_priority.o res_task_heap.o alma_term_search.o
	$(CC)  -o libalma.a tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o compute_priority.o res_task_heap.o alma_term_search.o alma.o
run:
	./alma.x demo/fc-test.pl
