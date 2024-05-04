# My first makefile
 
all: broker
 
broker: mqtt-broker.o
	gcc -o broker mqtt-broker.o utils.o
	
mqtt-broker.o: mqtt-broker.c utils.h
	gcc -c mqtt-broker.c utils.c
 
clean:
	rm -rf *.o broker 
