# Makefile for executable ep1

# *****************************************************
# Parameters to control Makefile operation

CC = gcc
CFLAGS = -ansi -pedantic -Wall -W -g -std=c99 -Wno-pointer-sign

# ****************************************************
# Entries to bring the executable up to date

ep1: ep1.c
	$(CC) $(CFLAGS) -o ep1 ep1.c

clean:
	rm -rf *.o *$ep1
