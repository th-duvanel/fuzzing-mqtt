#ifndef MESSAGE_H
#define MESSAGE_H

#include <util.h>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#define EXIT_MESSAGE 11

typedef struct {
    int packetType;
    int flags;
    int remainingSize;
    char * remaining;
} Message;

typedef struct {
    char * data;
    int size;
} Packet;

Packet createPacketFromMessage(Message message);

Message createMQTTMessageFromData(Packet raw);
#endif