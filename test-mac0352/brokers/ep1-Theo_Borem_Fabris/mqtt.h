#ifndef MQTT_EP1
#define MQTT_EP1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>    /* inet_pton function */
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <signal.h>
#include <pthread.h>

#include "queue.h"
#include "search.h"

/* defined types */
typedef unsigned char Byte;
typedef struct Queue Queue;

struct Client {
    pthread_t tid;
    int writepipefd;
};
typedef struct Client Client;
void clientPurge (Client *client);
int clientCompare (void *client1, void *client2);


struct Infrastructure {
    Search *clients;
    int fd;
};
typedef struct Infrastructure Infrastructure;

struct MessageStruct {
    Search *clients;
    Byte *topicname;
    ssize_t topicnamelen;
    Byte *message;
    ssize_t messagelen;
};
typedef struct MessageStruct MessageStruct;

/* MQTT Control Packet Type */
#define MQTT_CONNECT    1
#define MQTT_CONNACK    2
#define MQTT_PUBLISH    3
#define MQTT_SUBSCRIBE  8
#define MQTT_SUBACK     9
#define MQTT_PINGREQ   12
#define MQTT_PINGRESP  13
#define MQTT_DISCON    14

/* MQTT bytes lengths */
#define MQTT_CPTLEN 1
#define MQTT_PKTIDLEN 2
#define MQTT_TOPICSZLEN 2
#define MQTT_QOSLEN 1

/* Finite State Machine */
typedef unsigned char State;
#define STT_NEW        0x0
#define STT_CONNECTED  0x1
#define STT_SUBSCRIBED 0x2
#define STT_IS_CON(x) (((x) & STT_CONNECTED) != 0)
#define STT_IS_SUB(x) (((x) & STT_SUBSCRIBED) != 0)
#define SUCCESS 1
#define FAILURE -1

#define printErrnoMessage(message) fprintf(stderr, "mqttbroker: [ERROR] " message ": %s\n", strerror(errno))
#define printLog(...)              fprintf(stderr, "mqttbroker: " __VA_ARGS__)
#define printErrnoMessageClient(tid, message) fprintf(stderr, "mqttbroker: [Thread %ld ERROR] " message ": %s\n", tid, strerror(errno))
#define printLogClient(tid, format, ...) fprintf(stderr, "mqttbroker: [Thread %ld] " format, tid, __VA_ARGS__)
#define printLogClientError(tid, format, ...) fprintf(stderr, "mqttbroker: [Thread %ld ERROR] " format, tid, __VA_ARGS__)

ssize_t copyBytes (Byte *src, Byte *dst, ssize_t qtd);
void printLogBytesSequence (Byte *sequence, ssize_t size);
int calculateRemainingLength (int msgfd, ssize_t *recvmsglen);
Byte lengthToRemainingLength (size_t length, Byte remaininglen[4]);
int connectAndAcknowledge (int clientfd, pthread_t clientpid, ssize_t remaininglen, State *mstate);
int subscribeAndAcknowledge (Search *topics, int clientfd, pthread_t clientpid, ssize_t remaininglen);
int sendSUBACK (int clientfd, pthread_t clientpid, Queue *subscribedTopics, int packetid);
int sendMessageToPublish (int writefd, int clientfd, pthread_t clientpid, ssize_t remaininglen);
int topicsCompare (void *topic1, void *topic2);
void topicnamePurge (void *topicname);
MessageStruct *messageStructCreate(Search *clients, Byte *topicname, ssize_t topicnamelen, Byte *message, ssize_t messagelen);
void messageStructFree(MessageStruct *messageStruct);

#endif
