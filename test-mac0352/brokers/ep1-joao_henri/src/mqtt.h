#ifndef MQTT_H
#define MQTT_H

#include <sys/types.h>

/** ================================================================================================================= */

/**
 * These definitions are based on this documentation about MQTT 3.1.1:
 * http://www.steves-internet-guide.com/mqtt-protocol-messages-overview/#:~:text=The%20MQTT%20packet%20or%20message,)%20%2B%20Variable%20Header%20%2DExample%20PUBACK
 */

enum packet_type {
    CONNECT    = 0x1,
    CONNACK    = 0x2,

    PUBLISH    = 0x3,

    SUBSCRIBE  = 0x8,
    SUBACK     = 0x9,

    PINGREQ    = 0xc,
    PINGRESP   = 0xd,

    DISCONNECT = 0xe
};

/**
 * As stated in the documentation, every MQTT 3.1.1 packet has a "fixed header" like the diagram below.
 *
 * The first byte is divided into two parts:
 * - The 4 most significant bits (big endian) store the packet type
 * - The 4 less significant bits store some flags.
 *
 * Then, the bytes from 2 to 5 store the remaining packet length, i.e., variable header and payload.
 *
 * |--------|---------------|---------------|
 * | Bit    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 * |--------|---------------|---------------|
 * | Byte 1 | MQTT type     | Flags         |
 * |--------|-------------------------------|
 * | Byte 2 |                               |
 * | Byte 3 |           Remaining           |
 * | Byte 4 |           Length              |
 * | Byte 5 |                               |
 * |--------|---------------|---------------|
 */
typedef struct fixed_header {
    enum packet_type type;
    int length;
} fixed_header;

/**
 * Finds the type of the received packet and its remaining length.
 */
fixed_header* parse_fixed_header(unsigned char* packet);

/** ================================================================================================================= */

/**
 * Creates a CONNACK packet. It is hardcoded as such:
 * |--------|---------------|---------------|
 * | Bit    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
 * |--------|---------------|---------------|
 * | Byte 1 | CONNACK (0x2) | 0   0   0   0 |
 * |--------|-------------------------------|
 * | Byte 2 | Message Length    (2)         |
 * |--------|-------------------------------|
 * | Byte 3 | Acknowledge flags (0x00)      |
 * |--------|-------------------------------|
 * | Byte 4 | Return Code       (0x00)      |
 * |--------|---------------|---------------|
 */
u_int8_t* create_connack_packet();

/** ================================================================================================================= */

u_int8_t* create_pingresp_packet();

/** ================================================================================================================= */

typedef struct subscribe_packet {
    u_int16_t message_identifier;
    u_int16_t topic_length;
    char* topic;
} subscribe_packet;

subscribe_packet* parse_subscribe_packet(unsigned char* recvline);

u_int8_t* create_suback_packet(u_int16_t message_id, int success);

int subscribe_client(subscribe_packet* s);

void start_listener_child_process(int topic_id, int connfd);

/** ================================================================================================================= */

typedef struct publish_packet {
    u_int16_t topic_length;
    u_int16_t message_length;
    char* topic;
    char* message;
    unsigned char* raw_packet;
    u_int16_t raw_packet_length;
} publish_packet;

publish_packet* parse_publish_packet(fixed_header* h, unsigned char* recvline);

/** ================================================================================================================= */

#endif