CC = gcc
CFLAGS = -std=c99 -pedantic-errors -Wall -Werror -Wshadow -Wpedantic -g

all: alma_parser.x

alma_parser.x: alma_parser.o mpc.o alma_formula.o
	$(CC) alma_parser.o mpc.o alma_formula.o -o alma_parser.x

alma_parser.o: alma_parser.c
	$(CC) $(CFLAGS) -c alma_parser.c

mpc.o: mpc/mpc.c mpc/mpc.h
	$(CC) $(CFLAGS) -c mpc/mpc.c

alma_formula.o: alma_formula.c alma_formula.h
	$(CC) $(CFLAGS) -c alma_formula.c

clean:
	rm -f *.x *.o
