CC = gcc
CFLAGS = -std=c11 -pedantic-errors -Wall -Werror -Wshadow -Wpedantic -g

all: alma.x

alma.x: alma.o mpc.o alma_parser.o alma_formula.o alma_unify.o
	$(CC) alma.o mpc.o alma_parser.o alma_formula.o alma_unify.o -o alma.x

alma.o: alma.c mpc/mpc.h alma_parser.h alma_formula.h alma_unify.h
	$(CC) $(CFLAGS) -c alma.c

mpc.o: mpc/mpc.c mpc/mpc.h
	$(CC) $(CFLAGS) -c mpc/mpc.c

alma_parser.o: alma_parser.c mpc/mpc.h alma_parser.h
	$(CC) $(CFLAGS) -c alma_parser.c

alma_formula.o: alma_formula.c mpc/mpc.h alma_formula.h
	$(CC) $(CFLAGS) -c alma_formula.c

alma_unify.o: alma_unify.c alma_unify.h alma_formula.h
	$(CC) $(CFLAGS) -c alma_unify.c

clean:
	rm -f *.x *.o

run:
	./alma.x formulae.pl
