#ifndef MQTT_H
#define MQTT_H

#include <message.h>
#include<config.h>
#include <util.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

#define EXIT_MQTT 12

#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define SUBSCRIBE 8
#define SUBACK 9
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14

#define SPECIAL 15

typedef struct {
    char *name;
    int nameSize;
} Topic;

typedef struct {
    char *topicPath;
    int socketfd;
} ListenerArgs;

Message processMQTTMessage(Message message, int socketfd);

/*
cleanListener()
Sends a SIGINT signal if there's a listener thread
running.
*/
void cleanListener();
#endif