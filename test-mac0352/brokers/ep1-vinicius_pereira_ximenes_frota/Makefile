server: server.o
	gcc -o server server.o -lpthread

server.o: mac0352-servidor-exemplo-ep1.c
	gcc -o server.o mac0352-servidor-exemplo-ep1.c -c

clean:
	rm -rf *.o *~ server
