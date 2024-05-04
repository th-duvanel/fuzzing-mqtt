

#ifndef MQTT_H
#define MQTT_H
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>

#include <dirent.h>

#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define SUBSCRIBE 8
#define SUBACK 9
#define PINGREQ 12
#define DISCONNECT 14

int on_read(int connfd, unsigned char* recvline, int* fifofd, unsigned char* fifo_path);
int on_publish(unsigned char* recvline, unsigned char* packet, long line_length);
int on_subscribe(unsigned char* message, unsigned char* fifo_path);
int parse_packet(int connfd, unsigned char* recvline, long* offset, long* packet_length);
void rand_str(unsigned char *dest, size_t length);

void debug_print(const char* fmt, ...);

