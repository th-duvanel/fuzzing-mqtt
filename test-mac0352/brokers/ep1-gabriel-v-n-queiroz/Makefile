all: broker.bin

%.bin: %.c
	gcc -g -pthread -Wall -std=c11 $< -o $@