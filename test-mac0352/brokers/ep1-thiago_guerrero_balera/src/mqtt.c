#include <mqtt.h>

pthread_t listenerID = -1;

void handlePublish(Topic topic, Packet packet) {
    int topicfd;
    DIR *topicFolder;
    struct dirent *currentFile;
    char * filePath, * topicPath = malloc((topic.nameSize + strlen(TMP_FOLDER) + 2) * sizeof(char));

    sprintf(topicPath, "%s/%s", TMP_FOLDER, topic.name);
    if((topicFolder = opendir(topicPath)) == NULL) {
        if(errno == ENOENT) //No subscriber
            return;
        perror("open topic folder :(\n");
        exit(EXIT_MQTT);
    }

    while((currentFile = readdir(topicFolder)) != NULL) {
        if(currentFile->d_type != DT_FIFO)
            continue;

        filePath = malloc((strlen(topicPath) + strlen(currentFile->d_name) + 2));
        sprintf(filePath, "%s/%s", topicPath, currentFile->d_name);
    
        if((topicfd = open(filePath, O_WRONLY)) == -1) {
            perror("Publish fails! Closing connection.\n");
            close(topicfd);
            exit(EXIT_MQTT);
        }
        write(topicfd, packet.data, packet.size);
        free(filePath);
        close(topicfd);
    }
    
    if(closedir(topicFolder) == -1) {
        perror("closedir :(\n");
        exit(EXIT_MQTT);
    }

    free(topicPath);
}

void cleanUpThreadData(void *rawArgs) {
    ListenerArgs * args = (ListenerArgs *)rawArgs;
    free(args->topicPath);
    free(rawArgs);
}

void *listeningTopic(void *rawArgs) {
    pthread_cleanup_push(cleanUpThreadData, rawArgs);
    ListenerArgs args = *(ListenerArgs *)rawArgs;
    int topicfd;
    Packet input;

    input.data = malloc((MAXLINE + 1) * sizeof(char));
    bzero(input.data, (MAXLINE + 1));
    while((topicfd = getOrCreateTopicFile(args.topicPath, O_RDONLY)) != -1) {
        if((input.size = read(topicfd, input.data, MAXLINE)) == -1) {
            perror("read :(\n");
            exit(EXIT_MQTT);
        }
        write(args.socketfd, input.data, input.size);
        close(topicfd);
    }

    if(topicfd == -1) {
        perror("open specific topic pipe :(\n");
        exit(EXIT_MQTT);
    }
    pthread_cleanup_pop(1);
}

void handleSubscribe(Topic topic, int socketfd) {
    ListenerArgs *listenerArgs = malloc(sizeof(ListenerArgs));
    listenerArgs->topicPath = malloc((topic.nameSize + strlen(TMP_FOLDER) + 2) * sizeof(char));
    listenerArgs->socketfd = socketfd;
    sprintf(listenerArgs->topicPath, "%s/%s", TMP_FOLDER, topic.name);
    
    if (pthread_create(&listenerID, NULL, listeningTopic, listenerArgs) != 0) {
        perror("pthread :(\n");
        exit(EXIT_MQTT);
    }
}

unsigned char * getPacketIdentifier(Message *message) {
    checkSize((*message).remainingSize, 2, EXIT_MQTT);
    unsigned char * packetIdentifier = malloc(2 * sizeof(char));
    packetIdentifier[0] = (*message).remaining[0];
    packetIdentifier[1] = (*message).remaining[1];

    (*message).remaining+=2;

    return packetIdentifier;
}

Topic getTopicName(Message message) {
    checkSize(message.remainingSize, 2, EXIT_MQTT);

    Topic newTopic;
    int base = (int)pow(2, 8);
    newTopic.nameSize = message.remaining[0]*base + message.remaining[1];
    
    checkSize(message.remainingSize - 2, newTopic.nameSize, EXIT_MQTT);
    
    newTopic.name = malloc((newTopic.nameSize + 1) * sizeof(char));
    strncpy(newTopic.name, &message.remaining[2], newTopic.nameSize);
    newTopic.name[newTopic.nameSize] = 0;

    return newTopic;
}

Message processMQTTMessage(Message message, int socketfd) {
    Message response;
    bzero(&response, sizeof(response));
    
    switch (message.packetType) {
        case CONNECT:
            response.packetType = CONNACK;
            response.remainingSize = 2;
            response.remaining = malloc(2 * sizeof(char));
            bzero(response.remaining, 2);
            break;
        case PINGREQ:
            response.packetType = PINGRESP;
            break;
        case PUBLISH: ;
            Topic topicPub = getTopicName(message);
            handlePublish(topicPub, createPacketFromMessage(message));

            response.packetType = SPECIAL;
            break;
        case SUBSCRIBE: ;
            char *packetIdentifier = getPacketIdentifier(&message);
            Topic topicSub = getTopicName(message);
            handleSubscribe(topicSub, socketfd);

            response.packetType = SUBACK;
            response.remainingSize = 3;
            response.remaining = malloc(3 * sizeof(char));
            bzero(response.remaining, 3);
            strncatM(response.remaining, packetIdentifier, 2, 0);
            free(packetIdentifier);
            break;
        case DISCONNECT: ;
            response.packetType = SPECIAL;
            break;
        default:
            fprintf(stderr, "Invalid packet type! Closing connection.\n");
            exit(EXIT_MQTT);
            break;
    }

    return response;
}

void cleanListener() {
    if(listenerID != -1)
        pthread_cancel(listenerID);
}