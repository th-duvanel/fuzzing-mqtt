#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>

#include <pthread.h>

#include <fcntl.h>
#include <sys/stat.h>

#include <dirent.h> 


#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

struct linkedList {
    pthread_t thread;
    int fileDescriptor;
    char * topic;
    struct linkedList * next;
};

typedef struct linkedList LinkedList;

typedef struct {
    unsigned char type;
    unsigned char fixedHeaderFlags;
    unsigned char remainingLength;
    unsigned char * variableHeader;
    unsigned char * payload;
} Packet;

#define NONE 0
#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define SUBSCRIBE 8
#define SUBACK 9
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14
#define SUCCESS_SUBSCRIBE 0x00

Packet createPacket(char * recvline, int n) {
    Packet packet; 

    packet.type = ((unsigned char) recvline[0]) >> 4;
    packet.fixedHeaderFlags = ((unsigned char) recvline[0])  & 0b1111;
    packet.remainingLength = (unsigned char) recvline[1];

    if (packet.remainingLength > 0) {
        packet.variableHeader = malloc(packet.remainingLength * sizeof(unsigned char));

        for (int i = 0; i < packet.remainingLength; i++)
            packet.variableHeader[i] = (unsigned char) recvline[i + 2];
    }
    else  
        packet.variableHeader = NULL;


    packet.payload = NULL;

    return packet;
}

Packet createPingrespPacket() {
    Packet pingrespPacket;

    pingrespPacket.type = PINGRESP;
    pingrespPacket.fixedHeaderFlags = 0;
    pingrespPacket.remainingLength = 0;
    pingrespPacket.variableHeader = NULL;
    pingrespPacket.payload = NULL;

    return pingrespPacket;
}

Packet createConnackPacket() {
    Packet connackPacket;

    connackPacket.type = CONNACK;
    connackPacket.fixedHeaderFlags = 0;
    connackPacket.remainingLength = 2;

    connackPacket.variableHeader = malloc(2 * sizeof(unsigned char));

    connackPacket.variableHeader[0] = 0x00;
    connackPacket.variableHeader[1] = 0x00;

    connackPacket.payload = NULL;

    return connackPacket;
}

LinkedList * getLastNode(LinkedList * listOfTopicsNode) {
    while (listOfTopicsNode->next != NULL) 
        listOfTopicsNode = listOfTopicsNode->next;

    return listOfTopicsNode;
}

void freeListOfTopics(LinkedList * listOfTopicsNode) {
    int fileDescriptor;
    char path[MAXLINE + 1];
    char pid[MAXLINE + 1];

    if (listOfTopicsNode != NULL) {

        if (listOfTopicsNode->topic != NULL){
            strcpy(path, "/tmp/");
            strcat(path, "ep1");
            strcat(path, "/");
            mkdir(path, 0777);
            strcat(path, listOfTopicsNode->topic);
            mkdir(path, 0777);
            strcat(path, "/");
            sprintf(pid, "%d", getpid());
            strcat(path, pid);
            strcat(path, ".falcon");

            fileDescriptor = open(path, O_RDWR);
            write(fileDescriptor, "", 1);
            close(fileDescriptor);

            pthread_join(listOfTopicsNode->thread, NULL);

            unlink(path);

            free(listOfTopicsNode->topic);
        }
        freeListOfTopics(listOfTopicsNode->next);
        free(listOfTopicsNode);
    }
}

void * thread(void * arguments) {
    ssize_t n;
    unsigned char recvline[MAXLINE + 1];

    int fileDescriptor = ((int *) arguments)[0];
    int connfd = ((int *) arguments)[1];

    while ((n=read(fileDescriptor, recvline, MAXLINE)) > 1)
        write(connfd, recvline, n);

    close(fileDescriptor);
    free(arguments);

    return NULL;
}

void * addTopic(char * topic, LinkedList *listOfTopicsNode, int connfd) {
    int * arguments = malloc(2*(sizeof(int)));
    char path[MAXLINE + 1];
    char pid[MAXLINE + 1];

    strcpy(path, "/tmp/");
    strcat(path, "ep1");
    strcat(path, "/");
    mkdir(path, 0777);
    strcat(path, topic);
    mkdir(path, 0777);
    strcat(path, "/");
    sprintf(pid, "%d", getpid());
    strcat(path, pid);
    strcat(path, ".falcon");

    listOfTopicsNode->next = malloc(sizeof(LinkedList));
    listOfTopicsNode = listOfTopicsNode->next;

    if (mkfifo(path,0644) == -1) {
        perror("mkfifo :(\n");
    }

    listOfTopicsNode->fileDescriptor = open(path, O_RDWR);
    arguments[0] = listOfTopicsNode->fileDescriptor;
    arguments[1] = connfd;

    pthread_create(&(listOfTopicsNode->thread), NULL, thread, arguments);

    listOfTopicsNode->topic = topic;
    listOfTopicsNode->next = NULL;
}

void freePacket(Packet packet) {

    if (packet.variableHeader != NULL)
        free(packet.variableHeader);

    if (packet.payload != NULL)
        free(packet.payload);
}

Packet createSubackPacket(Packet subscribePacket, LinkedList * listOfTopicsNode, int connfd) {
    Packet subackPacket;

    subackPacket.type = SUBACK;
    subackPacket.fixedHeaderFlags = 0;
    subackPacket.remainingLength = 2;

    listOfTopicsNode = getLastNode(listOfTopicsNode);

    
    for (int i = 2; i < subscribePacket.remainingLength; i++) {
        int j;
        unsigned char size = subscribePacket.variableHeader[i] + subscribePacket.variableHeader[i + 1];
        i += 2;

        char *topic = malloc((size + 1)*sizeof(char));
        topic[size] = '\0';

        for (j = i; j < i + size; j++) {
            topic[j - i] = subscribePacket.variableHeader[j];
        }

        addTopic(topic, listOfTopicsNode, connfd);
        listOfTopicsNode = getLastNode(listOfTopicsNode);
        i = j;
        subackPacket.remainingLength++;
    }

    subackPacket.variableHeader = malloc(subackPacket.remainingLength*sizeof(unsigned char *));

    subackPacket.variableHeader[0] = subscribePacket.variableHeader[0];
    subackPacket.variableHeader[1] = subscribePacket.variableHeader[1];

    for (int i = 2; i < subackPacket.remainingLength; i++)
        subackPacket.variableHeader[i] = SUCCESS_SUBSCRIBE;

    subackPacket.payload = NULL;

    return subackPacket;
}

Packet createNonePacket() {
    Packet nonePacket;

    nonePacket.type = NONE;
    nonePacket.fixedHeaderFlags = 0;
    nonePacket.remainingLength = 0;
    nonePacket.payload = NULL;
    nonePacket.variableHeader = NULL;

    return nonePacket;
}

char * convertPacketToMessage(Packet packet, int *size) {
    char * message;
    *size = 0;

    if (packet.type == NONE) 
        return NULL;
    
    *size = 2 + packet.remainingLength;

    message = malloc((*size)*sizeof(char));
    message[0] = (packet.type << 4) | packet.fixedHeaderFlags;
    message[1] = packet.remainingLength;

    for (int i = 2; i < *size; i++)
        message[i] = (char) packet.variableHeader[i - 2];

    return message;
}

void publish(Packet publishPacket) {
    int pipe;
    int sizeOfMessage;
    char path[MAXLINE + 1];
    char pipePath[MAXLINE + 1];
    char remainingLength = (char) publishPacket.remainingLength;
    char msb = (char) publishPacket.variableHeader[0];
    char lsb = (char) publishPacket.variableHeader[1];
    int sizeOfTopic = msb + lsb;
    char begin = 2;
    char end = begin + sizeOfTopic;
    char topic[MAXLINE + 1];

    DIR *directory;
    struct dirent *file;
    

    for (char i = begin; i < end; i++)
        topic[i - begin] = publishPacket.variableHeader[i];
        
    topic[sizeOfTopic] = '\0';

    strcpy(path, "/tmp/");
    strcat(path, "ep1");
    strcat(path, "/");
    mkdir(path, 0777);
    strcat(path, topic);
    mkdir(path, 0777);
    strcat(path, "/");

    char * message = convertPacketToMessage(publishPacket, &sizeOfMessage);

    directory = opendir(path);
    if (directory) {
        while ((file = readdir(directory)) != NULL) {
            strcpy(pipePath, path);
            if (strcmp(file->d_name, ".") != 0 && strcmp(file->d_name, "..") != 0) {
                strcpy(pipePath, path);
                strcat(pipePath, file->d_name);
                pipe = open(pipePath, O_RDWR);
                write(pipe, message, sizeOfMessage);
                close(pipe);
            }
        }
        closedir(directory);
    }
    free(message);
}

int main (int argc, char **argv) {
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    pid_t childpid;
    unsigned char recvline[MAXLINE + 1];
    ssize_t n;

    LinkedList * listOfTopics = malloc(sizeof(LinkedList));
    listOfTopics->next = NULL;
    listOfTopics->topic =  NULL;
   
    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

	for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }

        if ( (childpid = fork()) == 0) {
            printf("[Uma conexão aberta]\n");
            close(listenfd);

            while ((n=read(connfd, recvline, MAXLINE)) > 0) {
                recvline[n]=0;
                Packet packet = createPacket(recvline, n);
                Packet packetToClient;

                packetToClient = createNonePacket();

                switch (packet.type) {

                    case CONNECT:
                        printf("[Connect do processo filho %d]\n\n", getpid());
                        packetToClient = createConnackPacket();
                        break;
                    case PUBLISH:
                        printf("[Publish do processo filho %d]\n\n", getpid());
                        publish(packet);
                        break;
                    case SUBSCRIBE:
                        printf("[Subscribe do processo filho %d]\n\n", getpid());
                        packetToClient = createSubackPacket(packet, listOfTopics, connfd);
                        break;
                    case DISCONNECT:
                        printf("[Disconnect do processo filho %d]\n\n", getpid());
                        break;
                    case PINGREQ:
                        printf("[Ping do processo filho %d]\n\n", getpid());
                        packetToClient = createPingrespPacket();
                        break;
                    default:
                        printf("[Comportamento não previsto - hexadecimal]\n\n: %02x, %d", packet.type, packet.type);
                        break;
                }
                int size;
                char * message = convertPacketToMessage(packetToClient, &size);

                if (message != NULL) {
                    write(connfd, message, size);
                    free(message);
                }
                freePacket(packet);
                freePacket(packetToClient);
                fflush(stdout);
            }
            freeListOfTopics(listOfTopics);
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
            close(connfd);
    }
    exit(0);
}
