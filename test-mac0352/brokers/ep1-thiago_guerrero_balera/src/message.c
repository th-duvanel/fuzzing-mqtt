#include <message.h>

int decodeRemainingLength(Packet *raw) {
    int value = 0, multiplier = 1;
    int maxValue = pow(128, 3);

    while((*raw).size > 0) {
        int hasMoreData = testBit((*raw).data[0], 8);
        int currentValue = ((*raw).data[0] & 127);
        value += currentValue * multiplier;

        (*raw).data++;
        (*raw).size--;
        if(multiplier > maxValue) {
            fprintf(stderr, "Size of variable header + payload exceed MQTT max length. Closing connection.\n");
            exit(EXIT_MESSAGE);
        }
        if(!hasMoreData)
            return value;

        multiplier *= 128;
    }

    fprintf(stderr, "Malformed packet! Closing connection.\n");
    exit(EXIT_MESSAGE);
}

Packet encodeRemainingLength(int remainingLength) {
    Packet encodedBytes;
    bzero(&encodedBytes, sizeof(encodedBytes));

    if(remainingLength == 0)
        encodedBytes.size = 1;

    encodedBytes.data = malloc(4 * sizeof(char));
    while(remainingLength > 0) {
        encodedBytes.size++;
        char encodedByte = remainingLength % 128;
        remainingLength /= 128;
        if(remainingLength > 0)
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

Packet createPacketFromMessage(Message message) {
    int readedBytes = 0;
    Packet packet;
    bzero(&packet, sizeof(packet));

    Packet encodedRemainingLength = encodeRemainingLength(message.remainingSize);
    int fixedHeaderSize;
    fixedHeaderSize = 1 + encodedRemainingLength.size;
    packet.size = fixedHeaderSize + message.remainingSize;
    packet.data = malloc(packet.size * sizeof(char));

    packet.data[0] = (message.packetType<<4 & 0b11110000) | (message.flags & 0b00001111);
    readedBytes++;

    strncatM(packet.data, encodedRemainingLength.data, encodedRemainingLength.size, readedBytes);
    readedBytes += encodedRemainingLength.size;

    strncatM(packet.data, message.remaining, message.remainingSize, readedBytes);
    readedBytes += message.remainingSize;

    free(encodedRemainingLength.data);
    return packet;
}

Message createMQTTMessageFromData(Packet raw) {
    checkSize(raw.size, 2, EXIT_MESSAGE);

    char controlByte = raw.data[0];
    int packetType, flags, remainingLength;
    char *remaining = NULL;
    
    packetType = (controlByte & 0b11110000)>>4;
    flags = (controlByte & 0b00001111);
    raw.data++;
    raw.size--;

    remainingLength = decodeRemainingLength(&raw);
    if(remainingLength > 0) {
        remaining = malloc(remainingLength * sizeof(char));
        strncatM(remaining, raw.data, remainingLength, 0);
    }

    return buildMessage(packetType, flags, remainingLength, remaining);
}