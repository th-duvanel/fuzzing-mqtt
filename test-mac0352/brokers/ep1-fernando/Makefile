CC=gcc
#CFLAGS=-Wall -ansi -pedantic -O2
CFLAGS=-Wall -pedantic -O2

all: ep1

ep1: functions.o ep1.o
		$(CC) $(CFLAGS) $^ -o $@ 

functions.o: functions.c
		$(CC) $(CFLAGS) -c $^ -o $@

ep1.o: ep1.c
		$(CC) $(CFLAGS) -c $^ -o $@

clean:	
		rm -f *.o ep1