#ifndef MQTT_H
#define MQTT_H

#include <constants.h>
#include <topic.h>
#include <listener.h>
#include <message.h>
#include <packet.h>
#include <util.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <pthread.h>

Message readMessage(Message message, int socketfd);

void terminateThread();

Packet messageToPacket(Message message);

Message packetToMessage(Packet raw);

#endif
