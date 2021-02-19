CC = gcc
CFLAGS = -std=c11 -pedantic-errors -Wall -Werror -Wshadow -Wpedantic -g -fPIC
NUMPY_DIR_PY2 = /usr/lib/python2.7/dist-packages/numpy/core/include/numpy
NUMPY_DIR_PY3 = /usr/local/lib/python3.6/dist-packages/numpy/core/include/numpy

TOMMY = tommyds/tommyds/

all: alma.x shared python

alma.x: alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o
	$(CC) alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o -o alma.x

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

alma_fif.o: alma_fif.c alma_kb.h alma_formula.h alma_unify.h alma_proc.h alma_fif.h tommy.h
	$(CC) $(CFLAGS) -c alma_fif.c

alma_backsearch.o: alma_backsearch.c alma_kb.h alma_formula.h alma_unify.h alma_backsearch.h tommy.h
	$(CC) $(CFLAGS) -c alma_backsearch.c

clean:
	rm -f *.x *.o *.so build/lib.linux-x86_64-2.7/alma.so

shared: alma.o tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o
	$(CC) -shared -o libalma.so tommyarray.o tommyhashlin.o tommyhash.o tommylist.o mpc.o alma_parser.o alma_formula.o alma_kb.o alma_unify.o alma_command.o alma_print.o alma_proc.o alma_fif.o alma_backsearch.o alma.o
	sudo cp libalma.so /usr/local/lib/libalma.so

python:
	sudo python2 setup.py install
	echo "\n\nMake sure to add /usr/local/lib to your library linking path: e.g. adding \"export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib\" to ~/.bashrc file"
run:
	./alma.x -f demo/fc-test.pl
