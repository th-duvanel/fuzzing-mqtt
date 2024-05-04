#include "mqtt.h"

void clientPurge (Client *client)
{
    free(client);
}

int clientCompare (void *client1, void *client2)
{
    return ((Client *)client1)->tid - ((Client *)client2)->tid;
}

void topicnamePurge (void *topicname)
{
    free ((char *)topicname);
}

int topicsCompare (void *topic1, void *topic2)
{
    return strcmp((char *)topic1, (char *)topic2);
}

MessageStruct *messageStructCreate(Search *clients, Byte *topicname, ssize_t topicnamelen, Byte *message, ssize_t messagelen)
{
    MessageStruct *messageStruct;

    if ((messageStruct = malloc(sizeof(MessageStruct))) == NULL)
        return NULL;
    messageStruct->clients = clients;
    messageStruct->topicname = topicname;
    messageStruct->topicnamelen = topicnamelen;
    messageStruct->message = message;
    messageStruct->messagelen = messagelen;
    return messageStruct;
}

void messageStructFree(MessageStruct *messageStruct)
{
    if (messageStruct == NULL)
        return ;
    free(messageStruct->message);
    free(messageStruct->topicname);
    free(messageStruct);
}

int sendMessageToPublish (int writefd, int clientfd, pthread_t clientpid, ssize_t remaininglen)
{
    Byte *remainingMessage;

    if((remainingMessage = malloc(remaininglen)) == NULL ||
            read(clientfd, remainingMessage, remaininglen) != remaininglen ||
            write(writefd, &clientpid, sizeof(pthread_t)) != sizeof(pthread_t) ||
            write(writefd, &remaininglen, sizeof(ssize_t)) != sizeof(ssize_t) ||
            write(writefd, remainingMessage, remaininglen) != remaininglen) {
        printErrnoMessageClient(clientpid, "Send message error");
        free(remainingMessage); return FAILURE;
    }
    /* printLogClient(clientpid, "Message with size %ld sent successfully to be published: ", remaininglen);
    printLogBytesSequence(remainingMessage, remaininglen); */
    printLogClient(clientpid, "Message with size %ld sent successfully to be published.\n", remaininglen);
    free(remainingMessage);
    return SUCCESS;
}

int subscribeAndAcknowledge (Search *topics, int clientfd, pthread_t clientpid, ssize_t remaininglen)
{
    Byte qosbyte, twobytes[2], *topicname, *qosAck;
    ssize_t topiclen; int packetid;
    Queue *subscribedTopics;

    /* read packet id */
    if (read(clientfd, twobytes, MQTT_PKTIDLEN) != MQTT_PKTIDLEN)
        return 0;
    remaininglen -= 2;
    packetid = 256 * twobytes[0] + twobytes[1];

    subscribedTopics = queueCreate(free);
    while (remaininglen > 0) {
        /* read topic name length */
        read(clientfd, &twobytes, MQTT_TOPICSZLEN);
        remaininglen -= 2;
        topiclen = 256 * twobytes[0] + twobytes[1];

        /* read topic name */
        topicname = malloc((topiclen + 1) * sizeof(Byte));
        if (topicname == NULL ||
                read(clientfd, topicname, topiclen * sizeof(Byte)) != topiclen) {
            free(topicname); queuePurge(subscribedTopics); return FAILURE;
        }
        remaininglen -= topiclen;
        topicname[topiclen] = 0;

        /* read topic QoS */
        if(read(clientfd, &qosbyte, MQTT_QOSLEN) != MQTT_QOSLEN) {
            free(topicname); queuePurge(subscribedTopics); return FAILURE;
        }

        /* store QoS at the queue */
        if ((qosAck = malloc(sizeof(Byte))) == NULL) {
            free(topicname); queuePurge(subscribedTopics); return FAILURE;
        }
        remaininglen -= 1;
        *qosAck = qosbyte;
        queuePush(subscribedTopics, qosAck);
        /* If the client is connected, then topics != NULL */
        if (searchAdd(topics, topicname, &clientpid) == SEARCH_FAILURE) {
            free(topicname); queuePurge(subscribedTopics); return FAILURE;
        }
        printLogClient(clientpid, "Client (packet id %d) subscribed at topic \"%s\" with QoS %d.\n",
                       packetid, topicname, qosbyte);

        printLogClient(clientpid, "Topic \"%s\" added at client topic list.\n", topicname);
    }
    return sendSUBACK(clientfd, clientpid, subscribedTopics, packetid);
}

int sendSUBACK (int clientfd, pthread_t clientpid, Queue *subscribedTopics, int packetid)
{
    Byte ackRemaininglen[4], *ackMessage, *qosAck, qtdbytes; 
    ssize_t index;

    qtdbytes = lengthToRemainingLength(2 + queueLength(subscribedTopics), ackRemaininglen);
    /*            MQTTCPType Remaining length   packet id + payload
     *                   |   ___|___    _______________|________________   */
    ackMessage = malloc((1 + qtdbytes + 2 + queueLength(subscribedTopics)) * sizeof(char));
    if (ackMessage == NULL)
        return FAILURE;
    ackMessage[0] = 0x90;
    for (index = 1; index <= qtdbytes; index++)
        ackMessage[index] = ackRemaininglen[index - 1];
    ackMessage[index] = packetid >> 8;
    ackMessage[index + 1] = packetid & 0x00ff;
    index += 2;
    while (!queueIsEmpty(subscribedTopics)) {
        qosAck = queuePop(subscribedTopics);
        ackMessage[index++] = *qosAck;
        free(qosAck);
    }
    write(clientfd, ackMessage, index);
    /* printLogClient(clientpid, "SUBACK (%ld bytes): ", index);
    printLogBytesSequence(ackMessage, index); */
    printLogClient(clientpid, "SUBACK (%ld bytes).\n", index);
    free(ackMessage);
    queuePurge(subscribedTopics);
    return SUCCESS;
}

int connectAndAcknowledge (int clientfd, pthread_t clientpid, ssize_t remaininglen, State *mstate)
{
    Byte MQTT_MSG_CONNACK[] = {0x20, 0x02, 0x0, 0x0};
    Byte *recvmsg; ssize_t recvmsglen;

    recvmsg = malloc(remaininglen * sizeof(char));
    if ((recvmsglen = read(clientfd, recvmsg, remaininglen)) == -1) {
        printLogClientError(clientpid, "Payload (%ld bytes) read error.\n", remaininglen);
        return FAILURE;
    }
    /* printLogClient(clientpid, "Client connected with payload (expected/received %ld/%ld bytes): ", remaininglen, recvmsglen);
    printLogBytesSequence(recvmsg, recvmsglen); */
    printLogClient(clientpid, "Client connected with payload (expected/received %ld/%ld bytes).\n", remaininglen, recvmsglen);
    free(recvmsg);
    write(clientfd, MQTT_MSG_CONNACK, sizeof(MQTT_MSG_CONNACK)); 
    *mstate = STT_CONNECTED;
    /* printLogClient(clientpid, "CONNACK (%ld bytes): ", sizeof(MQTT_MSG_CONNACK));
    printLogBytesSequence(MQTT_MSG_CONNACK, sizeof(MQTT_MSG_CONNACK)); */
    printLogClient(clientpid, "CONNACK (%ld bytes).\n", sizeof(MQTT_MSG_CONNACK));
    return SUCCESS;
}

Byte lengthToRemainingLength (size_t length, Byte remaininglen[4])
{
    Byte index = 0;

    do {
        remaininglen[index] = length & 0x7f;
        length = length >> 7;
        if (length != 0) 
            remaininglen[index] |= 0x80;
        index++;
    } while (length != 0 && index < 4);
    
    return index;
} 

int calculateRemainingLength (int msgfd, ssize_t *recvmsglen)
{
    int multiplier = 1, value = 0;
    char encodedByte;

    do {
        if (multiplier > 128*128*128 ||
                read(msgfd, &encodedByte, sizeof(char)) == -1)
            return FAILURE;
        value += (encodedByte & 127) * multiplier;
        multiplier *= 128;
    } while ((encodedByte & 128) != 0);
    
    *recvmsglen = value;
    return value;
}

void printLogBytesSequence (Byte *sequence, ssize_t size)
{
    ssize_t i;

    for (i = 0; i < size - 1; i++)
        fprintf(stderr, "%x ", sequence[i]);
    fprintf(stderr, "%x\n", sequence[i]);
}

ssize_t copyBytes (Byte *src, Byte *dst, ssize_t qtd)
{
    ssize_t index;

    for (index = 0; index < qtd; index++)
        dst[index] = src[index];

    return index;
}
