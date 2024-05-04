#include <mqtt.h>

pthread_t threadListener = -1;

Topic getTopic(Message message) {
    matchValues(
            message.remainingSize,
            2,
            EXIT_MQTT);

    Topic newTopic;
    int base = (int) pow(2, 8);
    newTopic.nameSize = message.remaining[0] * base + message.remaining[1];

    matchValues(
            message.remainingSize - 2,
            newTopic.nameSize,
            EXIT_MQTT);

    newTopic.name = malloc((newTopic.nameSize + 1) * sizeof(char));

    strncpy(
            newTopic.name,
            &message.remaining[2],
            newTopic.nameSize);

    newTopic.name[newTopic.nameSize] = 0;

    return newTopic;
}

Message publish(Message message, Message response) {
    Topic topic = getTopic(message);
    Packet packet = messageToPacket(message);

    response.packetType = PUBACK;

    int topicfd;
    DIR *topicFolder;
    struct dirent *currentFile;
    char *filePath, *topicPath = malloc((topic.nameSize + strlen(TMP_DIR) + 2) * sizeof(char));

    sprintf(topicPath, "%s/%s", TMP_DIR, topic.name);
    if ((topicFolder = opendir(topicPath)) == NULL) {
        if (errno == ENOENT) //No subscriber
            return response;
        perror("open topic folder :(\n");
        exit(EXIT_MQTT);
    }

    while ((currentFile = readdir(topicFolder)) != NULL) {
        if (currentFile->d_type != DT_FIFO)
            continue;

        filePath = malloc((strlen(topicPath) + strlen(currentFile->d_name) + 2));
        sprintf(filePath, "%s/%s", topicPath, currentFile->d_name);

        if ((topicfd = open(filePath, O_WRONLY)) == -1) {
            perror("Publish fails! Closing connection.\n");
            close(topicfd);
            exit(EXIT_MQTT);
        }
        write(topicfd, packet.data, packet.size);
        free(filePath);
        close(topicfd);
    }

    if (closedir(topicFolder) == -1) {
        perror("closedir :(\n");
        exit(EXIT_MQTT);
    }

    free(topicPath);

    return response;
}

void cleanThread(void *threadARgs) {
    Listener *args = (Listener *) threadARgs;
    free(args->topicPath);
    free(threadARgs);
}

void *listenTopic(void *rawArgs) {
    pthread_cleanup_push(cleanThread, rawArgs) ;
            Listener args = *(Listener *) rawArgs;
            int topicfd;
            Packet input;

            input.data = malloc((MAXLINE + 1) * sizeof(char));
            bzero(input.data, (MAXLINE + 1));
            while ((topicfd = getTopicPipe(args.topicPath, O_RDONLY)) != -1) {
                if ((input.size = read(topicfd, input.data, MAXLINE)) == -1) {
                    perror("read :(\n");
                    exit(EXIT_MQTT);
                }
                write(args.socketfd, input.data, input.size);
                close(topicfd);
            }

            if (topicfd == -1) {
                perror("erro abrindo topic para pipe: (\n");
                exit(EXIT_MQTT);
            }
    pthread_cleanup_pop(1);
}

unsigned char *getPacketIdentifier(Message *message) {
    matchValues((*message).remainingSize, 2, EXIT_MQTT);
    unsigned char *packetIdentifier = malloc(2 * sizeof(char));
    packetIdentifier[0] = (*message).remaining[0];
    packetIdentifier[1] = (*message).remaining[1];

    (*message).remaining += 2;

    return packetIdentifier;
}

Message subscribe(Message message, Message response, int socketfd) {
    char *packetIdentifier = getPacketIdentifier(&message);
    Topic topic = getTopic(message);

    Listener *listenerArgs = malloc(sizeof(Listener));
    listenerArgs->topicPath = malloc((topic.nameSize + strlen(TMP_DIR) + 2) * sizeof(char));
    listenerArgs->socketfd = socketfd;
    sprintf(listenerArgs->topicPath, "%s/%s", TMP_DIR, topic.name);

    if (pthread_create(&threadListener, NULL, listenTopic, listenerArgs) != 0) {
        perror("pthread :(\n");
        exit(EXIT_MQTT);
    }

    response.packetType = SUBACK;
    response.remainingSize = 3;
    response.remaining = malloc(3 * sizeof(char));
    bzero(response.remaining, 3);
    byteToBinary(response.remaining, packetIdentifier, 2, 0);
    free(packetIdentifier);

    return response;
}

Message connect(Message response) {
    response.packetType = CONNACK;
    response.remainingSize = 2;
    response.remaining = malloc(2 * sizeof(char));
    bzero(response.remaining, 2);
    return response;
}

Message pingreq(Message response) {
    response.packetType = PINGRESP;
    return response;
}

Message disconnect(Message response) {
    response.packetType = RESERVED;
    return response;
}

Message readMessage(Message message, int socketfd) {
    Message response;
    bzero(&response, sizeof(response));

    switch (message.packetType) {
        case CONNECT:
            return connect(response);
        case PINGREQ:
            return pingreq(response);
        case PUBLISH:
            return publish(message, response);
        case SUBSCRIBE:
            return subscribe(message, response, socketfd);
        case DISCONNECT:
            return disconnect(response);
        default:
            fprintf(stderr, "Tipo de pacote invalido! Terminando conexão.\n");
            exit(EXIT_MQTT);
    }
}

void terminateThread() {
    if (threadListener != -1) {
        pthread_cancel(threadListener);
    }
}

int decodeRemainingLength(Packet *raw) {
    int value = 0, multiplier = 1;
    int maxValue = pow(128, 3);

    while ((*raw).size > 0) {
        int hasMoreData = testBit((*raw).data[0], 8);
        int currentValue = ((*raw).data[0] & 127);
        value += currentValue * multiplier;

        (*raw).data++;
        (*raw).size--;
        if (multiplier > maxValue) {
            fprintf(stderr, "Size of variable header + payload exceed MQTT max length. Closing connection.\n");
            exit(EXIT_MESSAGE);
        }
        if (!hasMoreData)
            return value;

        multiplier *= 128;
    }

    fprintf(stderr, "Malformed packet! Closing connection.\n");
    exit(EXIT_MESSAGE);
}

Packet encodeRemainingLength(int remainingLength) {
    Packet encodedBytes;
    bzero(&encodedBytes, sizeof(encodedBytes));

    if (remainingLength == 0)
        encodedBytes.size = 1;

    encodedBytes.data = malloc(4 * sizeof(char));
    while (remainingLength > 0) {
        encodedBytes.size++;
        char encodedByte = remainingLength % 128;
        remainingLength /= 128;
        if (remainingLength > 0)
            encodedByte = encodedByte & 128;

        encodedBytes.data[encodedBytes.size - 1] = encodedByte;
    }

    return encodedBytes;
}

Message buildMessage(int packetType, int flags, int size, char *remaining) {
    Message message;
    bzero(&message, sizeof(message));

    message.packetType = packetType;
    message.flags = flags;
    message.remainingSize = size;
    message.remaining = remaining;
    return message;
}

Packet messageToPacket(Message message) {
    int readedBytes = 0;
    Packet packet;
    bzero(&packet, sizeof(packet));

    Packet encodedRemainingLength = encodeRemainingLength(
            message.remainingSize
            );
    int fixedHeaderSize;

    fixedHeaderSize = 1 + encodedRemainingLength.size;
    packet.size = fixedHeaderSize + message.remainingSize;
    packet.data = malloc(packet.size * sizeof(char));

    packet.data[0] = (message.packetType << 4 & 0b11110000) | (message.flags & 0b00001111);
    readedBytes++;

    byteToBinary(
            packet.data,
            encodedRemainingLength.data,
            encodedRemainingLength.size,
            readedBytes);
    readedBytes += encodedRemainingLength.size;

    byteToBinary(
            packet.data,
            message.remaining,
            message.remainingSize,
            readedBytes);
    readedBytes += message.remainingSize;

    free(encodedRemainingLength.data);
    return packet;
}

Message packetToMessage(Packet raw) {
    matchValues(raw.size, 2, EXIT_MESSAGE);

    char controlByte = raw.data[0];
    int packetType, flags, remainingLength;
    char *remaining = NULL;

    packetType = (controlByte & 0b11110000) >> 4;
    flags = (controlByte & 0b00001111);
    raw.data++;
    raw.size--;

    remainingLength = decodeRemainingLength(&raw);
    if (remainingLength > 0) {
        remaining = malloc(remainingLength * sizeof(char));
        byteToBinary(remaining, raw.data, remainingLength, 0);
    }

    return buildMessage(packetType, flags, remainingLength, remaining);
}
