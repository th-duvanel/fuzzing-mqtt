#ifndef EP1_CONSTANTS_H
#define EP1_CONSTANTS_H

#define EXIT_UTIL 10
#define EXIT_MESSAGE 11
#define EXIT_MQTT 12

// packet types
#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define PUBACK 4
#define SUBSCRIBE 8
#define SUBACK 9
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14
#define RESERVED 15

#define TMP_DIR "/tmp/ep1-mac0352"
#define LISTENQ 1
#define MAXLINE 65535

#define testBit(value, bit) (value & (1 << bit))

#endif // EP1_CONSTANTS_H
