#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>    /* inet_pton function */
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>

#include "mqtt.h"

/* Network configs */
#define LQUEUELEN  2000
#define IPADDRLEN  15

/* Mutex to ensure mutual exclusion when adding or removing clients from the
 * client table.
 */
pthread_mutex_t clientTableLock;

void sendMessageToClientPipe (void *element, void *data)
{
    MessageStruct *messageStruct;
    ssize_t topicnamelen, messagelen;
    Byte *message, *topicname;
    Client *client; int writefd;

    if (element == NULL || data == NULL)
        return ;
    client = (Client *)element;
    writefd = client->writepipefd;
    messageStruct = (MessageStruct *)data;
    topicname = messageStruct->topicname; topicnamelen = messageStruct->topicnamelen;
    message = messageStruct->message; messagelen = messageStruct->messagelen;
    if (write(writefd, &topicnamelen, sizeof(ssize_t)) != sizeof(ssize_t)
            || write(writefd, topicname,  topicnamelen) != topicnamelen
            || write(writefd, &messagelen, sizeof(ssize_t)) != sizeof(ssize_t)
            || write(writefd, message,  messagelen) != messagelen)
        printErrnoMessageClient(client->tid, "Error while client pipe write");
}

void readPipeMessage (int pipefd, pthread_t clientpid, int socketfd, Search *topics)
{
    Byte *topicname, *message, *sendmessage, remaininglen[4], bytesTopicNamelen[2];;
    ssize_t topicnamelen, messagelen, sendmessagelen;
    int remaininglenSize; 

    if (read(pipefd, &topicnamelen, sizeof(ssize_t)) != sizeof(ssize_t)
            || (topicname = malloc(topicnamelen + 1)) == NULL
            || read(pipefd, topicname,  topicnamelen) != topicnamelen
            || read(pipefd, &messagelen, sizeof(ssize_t)) != sizeof(ssize_t)
            || (message = malloc(messagelen)) == NULL
            || read(pipefd, message,  messagelen) != messagelen) {
        printErrnoMessageClient(clientpid, "Error while pipe read");
        free(topicname); free(message); return ;
    }
    topicname[topicnamelen] = 0;
    if (searchFind(topics, topicname) != NULL
            && (remaininglenSize = lengthToRemainingLength(2 + topicnamelen + messagelen, remaininglen)) < 4
            && (sendmessage = malloc(1 + remaininglenSize + 2 + topicnamelen + messagelen)) != NULL) {
        sendmessage[0] = 0x30; sendmessagelen = 1 + remaininglenSize + 2 + topicnamelen + messagelen;
        bytesTopicNamelen[0] = (topicnamelen & 0xff00) >> 8;
        bytesTopicNamelen[1] = topicnamelen & 0xff;
        if (copyBytes(remaininglen, &sendmessage[1], remaininglenSize) != remaininglenSize
                || copyBytes(bytesTopicNamelen, &sendmessage[1 + remaininglenSize], 2) != 2
                || copyBytes(topicname, &sendmessage[1 + remaininglenSize + 2], topicnamelen) != topicnamelen
                || copyBytes(message, &sendmessage[1 + remaininglenSize + 2 + topicnamelen], messagelen) != messagelen
                || write(socketfd, sendmessage, sendmessagelen) != sendmessagelen) {
            printErrnoMessageClient(clientpid, "Error");
            printLogClient(clientpid, "Failed to send message from topic %s.\n", topicname);
        }
        else {
            printLogClient(clientpid, "Message from topic %s sent successfully.\n", topicname);
        }
        free(sendmessage);
    }
    free(topicname); free(message);
}

void *spreadPublisherMessage (void *data)
{
    MessageStruct *messageStruct;
    Search *clients;

    if (data == NULL)
        pthread_exit(NULL);
    messageStruct = (MessageStruct *)data; clients = messageStruct->clients;
    if (pthread_mutex_lock(&clientTableLock) != 0) {
        printErrnoMessage("Mutex lock error");
        pthread_exit(NULL);
    }
    searchApplyFunctionForEachElement(clients, sendMessageToClientPipe, data);
    pthread_mutex_unlock(&clientTableLock);
    messageStructFree(messageStruct);
    pthread_exit(NULL);
}

int publishMessage (pthread_t clientpid, int readfd, Search *clients, ssize_t remaininglen)
{
    ssize_t topicnamelen, messagelen;
    Byte twobytes[2], *topicname, *message = NULL;
    pthread_t temptid; MessageStruct *publishMessageStruct;

    if (read(readfd, twobytes, 2) != 2) {
        printErrnoMessageClient(clientpid, "Read error: parameters from client message.\n");
        return FAILURE;
    }
    topicnamelen = 256 * twobytes[0] + twobytes[1];
    if ((topicname = malloc(topicnamelen + 1)) == NULL
            || read(readfd, topicname, topicnamelen) != topicnamelen) {
        printErrnoMessageClient(clientpid, "Read error: topic name from client message.\n");
        free(topicname); return FAILURE;
    }
    /* As this server deals only with QoS 0, there is no packet id to read */
    messagelen = remaininglen - 2 - topicnamelen;
    if (messagelen < 0
            || (message = malloc(messagelen + 1)) == NULL
            || read(readfd, message, messagelen) != messagelen) {
        printErrnoMessageClient(clientpid, "Read error: message from client message.\n");
        free(message); free(topicname); return FAILURE;
    }
    topicname[topicnamelen] = 0; message[messagelen] = 0;
    printLogClient(clientpid, "Successfully read message '%s' of topic '%s' from process %ld.\n", message, topicname, clientpid);
    if ((publishMessageStruct = messageStructCreate(clients, topicname, topicnamelen, message, messagelen)) == NULL
            || pthread_create(&temptid, NULL, spreadPublisherMessage, (void *) publishMessageStruct) != 0) {
        printLogClient(clientpid, "Error during PUBLISH (%ld bytes).\n", remaininglen);
        free(message); free(topicname); free(publishMessageStruct);
        return FAILURE;
    }
    pthread_detach(temptid);
    printLogClient(clientpid, "Thread %ld assigned to spread the received message at time %ld.\n", temptid, time(NULL));
    return SUCCESS;
}

/* MQTT aplication layer
 * byte 1:  7-4 (MQTT Control Packet type);
 *          3-0 (Flags specific to each MQTT Cont. Pack. type, e.g., QoS).
 * byte 2-k - remaining length, k < 2+4, at most 4 bytes:
 *          Number of bytes remaining within the current packet,
 *          including data in variable header and the payload.
 *          The Remaining Length is encoded using a variable
 *          length encoding scheme which uses a single byte for
 *          values up to 127.
 *          Larger values are handled as follows.
 *          The least significant seven bits of each byte
 *          encode the data, and the most significant bit is
 *          used to indicate that there are following bytes in
 *          the representation. Thus each byte encodes 128
 *          values and a "continuation bit". The maximum number
 *          of bytes in the Remaining Length field is four.
 * byte k-EOF: variable header and the payload (they depend on
 *             MQTT CP type).
 * For example: 
 * 18 bytes read: 10 10 0 4 4d 51 54 54 4 2 0 3c 0 4 73 75 62 31
 *                -- -- ------------------------------------- EOF
 *                Remaining Length = 16
 */
void *clientmain (void *infra)
{
    ssize_t remaininglen, status; Byte mqttcptype; State mstate;
    int clientfd, clientpipe[2], pipeqtd; struct pollfd fds[2];
    Search *clients, *topics = NULL; Client *client = NULL;
    Byte MQTT_MSG_PINGRESP[] = {0xd0, 0x00};
    pthread_t clientpid;

    if (infra == NULL)
        pthread_exit(NULL);
    clientfd = ((Infrastructure *)infra)->fd;
    clients = ((Infrastructure *)infra)->clients;
    free(infra);
    clientpid = pthread_self(); mstate = STT_NEW;
    bzero(fds, 2*sizeof(struct pollfd));
    pipeqtd = 1; fds[0].fd = clientfd; fds[0].events = POLLIN;
    while (1) {
        status = poll(fds, pipeqtd, -1);
        if (status == -1) {
            printErrnoMessageClient(clientpid, "Poll error");
            goto failure;
        }
        if ((fds[0].revents & POLLIN) != 0) {
            status = read(clientfd, &mqttcptype, MQTT_CPTLEN);
            if (status == 0) break;
            if (status == -1) {
                printErrnoMessageClient(clientpid, "Read error");
                goto failure;
            }
            mqttcptype = (mqttcptype >> 4) & 0x0f;
            if (calculateRemainingLength(clientfd, &remaininglen) == FAILURE) {
                printLogClientError(clientpid, "Malformed Packet of MQTT Control type %d.\n", mqttcptype);
                goto failure;
            }
            printLogClient(clientpid, "MQTT Control type %d.\n", mqttcptype);
            if (mqttcptype == MQTT_CONNECT) {
                if (STT_IS_CON(mstate)) {
                    printLogClientError(clientpid, "Protocol violation: Second CONNECT packet (remaining length %ld).\n", remaininglen);
                    goto failure;
                }
                if (connectAndAcknowledge(clientfd, clientpid, remaininglen, &mstate) != SUCCESS) {
                    goto failure;
                }
            }
            else if (mqttcptype == MQTT_SUBSCRIBE) {
                if (!STT_IS_CON(mstate)) {
                    printLogClientError(clientpid, "Client unconnected tried to subscribe with payload size %ld.\n", remaininglen);
                    continue;
                }
                if (!STT_IS_SUB(mstate)) {
                    if ((client = malloc(sizeof(Client))) == NULL
                            || pipe(clientpipe) == -1) {
                        printErrnoMessageClient(clientpid, "Fail to allocate resources (struct Client or pipe) to the connected client");
                        goto failure;
                    }
                    client->tid = clientpid;
                    client->writepipefd = clientpipe[1];
                    if (pthread_mutex_lock(&clientTableLock) != 0) {
                        printErrnoMessageClient(clientpid, "Mutex lock error");
                        goto failure;
                    }
                    if (searchAdd(clients, &client->tid, client) == SEARCH_FAILURE
                            || (topics = searchCreate(topicsCompare, NULL, (void(*)(void*))topicnamePurge)) == NULL) {
                        printErrnoMessageClient(clientpid, "Fail to allocate resources (add or topic list) to the connected client");
                        pthread_mutex_unlock(&clientTableLock);
                        goto failure;
                    }
                    pthread_mutex_unlock(&clientTableLock);
                    fds[1].fd = clientpipe[0];
                    fds[1].events = POLLIN;
                    pipeqtd = 2;
                    mstate |= STT_SUBSCRIBED;
                    printLogClient(clientpid, "Client added on connected client list with write pipe file descriptor %d.\n", clientpipe[1]);
                }
                if ((status = subscribeAndAcknowledge(topics, clientfd, clientpid, remaininglen)) == 0) {
                    printLogClientError(clientpid, "Protocol violation: SUBSCRIBE packet with no payload (size %ld).\n", remaininglen);
                    goto failure;
                }
                else if (status == FAILURE) {
                    printLogClientError(clientpid, "Read error with payload size %ld.\n", remaininglen);
                    goto failure;
                }
            }
            else if (mqttcptype == MQTT_PINGREQ) {
                write(clientfd, MQTT_MSG_PINGRESP, sizeof(MQTT_MSG_PINGRESP));
                /* printLogClient(clientpid, "PINGRESP (%ld bytes): ", sizeof(MQTT_MSG_PINGRESP));
                printLogBytesSequence(MQTT_MSG_PINGRESP, sizeof(MQTT_MSG_PINGRESP)); */
                printLogClient(clientpid, "PINGRESP (%ld bytes).\n", sizeof(MQTT_MSG_PINGRESP));
            }
            else if (mqttcptype == MQTT_DISCON) {
                printLogClient(clientpid, "Received DISCONNECT with payload size %ld.\n", remaininglen);
                break;
            }
            else if (mqttcptype == MQTT_PUBLISH) {
                if (publishMessage(clientpid, clientfd, clients, remaininglen) == FAILURE) {
                    printLogClientError(clientpid, "Fail to PUBLISH (%ld bytes).\n", remaininglen);
                    goto failure;
                }
            }
        }
        if (pipeqtd == 2 && (fds[1].revents & POLLIN) != 0) {
            printLogClient(clientpid, "Received pipe %d message.\n", clientpipe[0]);
            readPipeMessage(clientpipe[0], clientpid, clientfd, topics);
        }
    }
    close(clientfd);
    if (STT_IS_SUB(mstate)) {
        if (pthread_mutex_lock(&clientTableLock) != 0) {
            printErrnoMessageClient(clientpid, "Mutex lock error");
            pthread_exit(NULL);
        }
        searchDelete(clients, &client->tid);
        pthread_mutex_unlock(&clientTableLock);
        searchPurge(topics);
        close(clientpipe[0]); close(clientpipe[1]);
    }
    printLogClient(clientpid, "Connection closed at time %ld.\n", time(NULL));
    pthread_exit(NULL);

failure:
    close(clientfd);
    if (STT_IS_SUB(mstate)) {
        if (pthread_mutex_lock(&clientTableLock) != 0) {
            printErrnoMessageClient(clientpid, "Mutex lock error");
            pthread_exit(NULL);
        }
        searchDelete(clients, &client->tid);
        pthread_mutex_unlock(&clientTableLock);
        searchPurge(topics);
        close(clientpipe[0]); close(clientpipe[1]);
    }
    printLogClient(clientpid, "Connection aborted at time %ld.\n", time(NULL));
    pthread_exit(NULL);
}

/* broker terminal and data structure control */
int brokerController (int port, int listenfd)
{
    struct sockaddr_in servaddr; socklen_t servaddrlen;
    pthread_t clientThread; Infrastructure *infraThread;
    char clientipaddr[IPADDRLEN + 1], temp[50];
    int status, clientfd; struct pollfd fds[2];
    Search *clients;

    if ((clients = searchCreate(clientCompare, NULL, (void (*)(void*)) clientPurge)) == NULL
            || pthread_mutex_init(&clientTableLock, NULL) != 0) {
        printErrnoMessage("Fail to allocate client table resources");
        close(listenfd);
        return EXIT_FAILURE;
    }

    servaddrlen = sizeof(servaddr);
    bzero(fds, 2*sizeof(struct pollfd));
    fds[0].fd = listenfd; fds[0].events = POLLIN;
    fds[1].fd = 0; fds[1].events = POLLIN;
    printLog("Broker accepting connections at port %d.\n", port);
    while (1) {
        status = poll(fds, 2, -1);
        if (status == -1) {
            printErrnoMessage("Poll error");
            break;
        }
        if ((fds[0].revents & POLLIN) != 0) {
            if ((clientfd = accept(listenfd, (struct sockaddr *) &servaddr, &servaddrlen)) == -1) {
                printErrnoMessage("Accept error");
                continue;
            }
            if ((infraThread = malloc(sizeof(Infrastructure))) == NULL
                    || (infraThread->fd = clientfd) != clientfd
                    || (infraThread->clients = clients) != clients
                    || pthread_create(&clientThread, NULL, clientmain, (void *) infraThread) != 0) {
                printLog("Failed to assign a thread to client from %s.\n", 
                         inet_ntop(AF_INET, &(servaddr.sin_addr.s_addr), clientipaddr, IPADDRLEN));
                continue;
            }
            pthread_detach(clientThread);
            printLog("Thread %ld assigned to client from %s at time %ld.\n", clientThread,
                     inet_ntop(AF_INET, &(servaddr.sin_addr.s_addr), clientipaddr, IPADDRLEN), time(NULL));
        }
        if ((fds[1].revents & POLLIN) != 0) {
            if ((status = scanf("%[^\n]", temp)) == EOF)
                break;
            temp[0] = getc(stdin);
        }
    }
    close(listenfd);
    pthread_mutex_lock(&clientTableLock);
    searchPurge(clients);
    pthread_mutex_unlock(&clientTableLock);
    pthread_mutex_destroy(&clientTableLock);
    return EXIT_SUCCESS;
}

int main (int argc, char **argv)
{
    /* struct sockaddr_in is defined in /usr/include/netinet/in.h, which
     * also define htons-type functions, INADDR_ANY, etc. */
    struct sockaddr_in servaddr; socklen_t servaddrlen; in_port_t port;
    int listenfd;
    struct rlimit limits;

    if (argc > 0 && argc != 2) {
        printLog("Usage: %s <port_number>.\n", argv[0]);
        return EXIT_FAILURE;
    }

    getrlimit(RLIMIT_NOFILE, &limits);
    /* the limit of open files of your machine can be found using: ulimit -n */
    printLog("An estimate of the maximum number of subscribers is %ld (the limit of open file divided by three).\n", limits.rlim_cur / 3);
    servaddrlen = sizeof(servaddr);
    bzero(&servaddr, servaddrlen);
    servaddr.sin_family = AF_INET;
    port = atoi(argv[1]);
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1
            || bind(listenfd, (struct sockaddr *) &servaddr, servaddrlen) == -1
            || listen(listenfd, LQUEUELEN) == -1) {
        printErrnoMessage("Socket error");
        return EXIT_FAILURE;
    }
     
    return brokerController(port, listenfd);
}

