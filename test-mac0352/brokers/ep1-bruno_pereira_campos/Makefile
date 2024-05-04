CC = gcc

CFLAGS = -Wall -Wextra -O3

all: mqttserver

mqttserver: md5.c broker.c 
	@$(CC) $(CFLAGS) -o broker broker.c md5.c 

clean: 
	rm broker