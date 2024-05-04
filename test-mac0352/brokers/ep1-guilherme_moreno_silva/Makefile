all: DEBUG=0 
all: broker

debug: DEBUG=1
debug: broker
	
broker: broker.o mqtt.o
	gcc -o broker broker.o mqtt.o

broker.o: broker.c mqtt.h
	gcc -o broker.o broker.c -c -Wall -std=c99

mqtt.o: mqtt.c mqtt.h
	gcc -o mqtt.o mqtt.c -DDEBUG=$(DEBUG) -c -Wall -std=c99

clean:
	rm -rf *.o broker