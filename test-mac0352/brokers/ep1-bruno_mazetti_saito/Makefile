

# Name of the project
PROJ_NAME=mqtt-broker
 
# .c files
C_SOURCE=$(wildcard *.c)
 
# .h files
H_SOURCE=$(wildcard *.h)
 
# Object files
OBJ=$(C_SOURCE:.c=.o)
 
# Compiler
CC=gcc
 
# Flags for compiler
CC_FLAGS=-c -W -Wall -ansi -pedantic

MATH_LIB_FLAG+=-lm





#
# Compilation and linking
#
all: $(PROJ_NAME) clean

$(PROJ_NAME): $(OBJ)
	$(CC) -o $@ $^ $(MATH_LIB_FLAG)

%.o: %.c %.h
	$(CC) -o $@ $< $(CC_FLAGS) $(MATH_LIB_FLAG)

main.o: main.c $(H_SOURCE)
	$(CC) -o $@ $< $(CC_FLAGS)

clean:
	rm -rf *.o