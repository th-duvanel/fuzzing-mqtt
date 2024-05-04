CCFLAGS = -Wall
LINKER = -lm #	 Necessário para a utilização da biblioteca math.
CC = gcc

ep1_broker : ep1_broker.c
	$(CC) $(CCFLAGS) -o ep1_broker ep1_broker.c $(LINKER)


# Para remover os arquivos criados no diretório /tmp
.PHONY : clean
clean :
		-rm -f /tmp/broker11795356*