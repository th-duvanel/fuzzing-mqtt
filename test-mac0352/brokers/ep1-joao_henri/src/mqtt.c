#include "mqtt.h"
#include "topic.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>

/** ================================================================================================================= */

fixed_header* parse_fixed_header(unsigned char* recvline) {
    fixed_header* header = malloc(sizeof(fixed_header));

    // Here, we get only the 4 most significant bits, as these
    // represent the packet type. Any flags are ignored.
    header->type = recvline[0] >> 4;

    header->length = recvline[1];

    return header;
}

/** ================================================================================================================= */

u_int8_t* create_connack_packet() {
    int i = 0;
    int length = 4;

    u_int8_t* connack_packet = malloc((length + 1) * sizeof(u_int8_t));

    // Packet type + flags
    connack_packet[i++] = CONNACK << 4;

    // Message Length
    connack_packet[i++] = 2;

    // Acknowledge flags
    connack_packet[i++] = 0x00;

    // Return code
    connack_packet[i++] = 0x00;

    return connack_packet;
}

/** ================================================================================================================= */

u_int8_t* create_pingresp_packet() {
    int i = 0;
    int length = 2;

    u_int8_t* connack_packet = malloc((length + 1) * sizeof(u_int8_t));

    // Packet type + flags
    connack_packet[i++] = PINGRESP << 4;

    // Message Length
    connack_packet[i++] = 0;

    return connack_packet;
}

/** ================================================================================================================= */

subscribe_packet* parse_subscribe_packet(unsigned char* recvline) {
    subscribe_packet* s = malloc(sizeof(subscribe_packet));

    // We hardcode i = 2 so we skip the fixed header in recvline.
    int i = 2;

    s->message_identifier = (1 << 8) * recvline[i] + recvline[i + 1]; i += 2;
    s->topic_length       = (1 << 8) * recvline[i] + recvline[i + 1]; i += 2;

    s->topic              = malloc((s->topic_length + 1) * sizeof(char));
    memcpy(s->topic, &recvline[i], s->topic_length);
    s->topic[s->topic_length] = 0;

    return s;
}

int subscribe_client(subscribe_packet* s) {
    int topic_id = get_topic_id_by_name(s->topic);

    if (topic_id == -1) {
        topic_id = create_topic(s);

        if (topic_id == -1) {
            return -1;
        }
    }

    return topic_id;
}

u_int8_t* create_suback_packet(u_int16_t message_id, int success) {
    int i = 0;
    int length = 5;

    u_int8_t* suback_packet = malloc((length + 1) * sizeof(u_int8_t));

    // Packet type + flags
    suback_packet[i++] = SUBACK << 4;

    // Message Length
    suback_packet[i++] = 3;

    // Message ID
    suback_packet[i++] = (u_int8_t)(message_id >> 8);
    suback_packet[i++] = (u_int8_t)message_id;

    // Granted QoS is "Fire and Forget"
    if (success == 1) {
        suback_packet[i++] = 0x00;
    } else {
        suback_packet[i++] = 0x80;
    }

    return suback_packet;
}

void start_listener_child_process(int topic_id, int connfd) {
    if ((fork()) == 0) {
        int client_offset = topics.current_offset[topic_id];

        while (1) {
            if (client_offset != topics.current_offset[topic_id]) {
                client_offset = (client_offset + 1) % TOPIC_MESSAGE_RETENTION_QUANTITY;
                write(connfd, topics.messages[topic_id][client_offset], topics.messages_length[topic_id][client_offset]);
            }
            usleep(100);
        }
    }
}

/** ================================================================================================================= */

publish_packet* parse_publish_packet(fixed_header* h, unsigned char* recvline) {
    publish_packet* p = malloc(sizeof(publish_packet));

    // We hardcode i = 2 so we skip the fixed header in recvline.
    int i = 2;
    p->topic_length       = (1 << 8) * recvline[i] + recvline[i + 1]; i += 2;
    p->topic              = malloc((p->topic_length + 1) * sizeof(char));
    memcpy(p->topic, &recvline[i], p->topic_length);
    p->topic[p->topic_length] = 0;

    i += p->topic_length;
    p->message_length = h->length - p->topic_length - 2;

    p->message = malloc((p->message_length + 1) * sizeof(char));
    memcpy(p->message, &recvline[i], p->message_length);
    p->message[p->message_length] = 0;

    // This raw_packet property is used to fix a weird bug where recvline contains both
    // PUBLISH and DISCONNECT packages. When this happens, we know the DISCONNECT is from
    // a publisher and can be ignored.
    // What we do here is consider only the package length in the fixed header + the two bytes
    // from the fixed header.
    p->raw_packet_length = h->length + 2;
    p->raw_packet = malloc(p->raw_packet_length * sizeof (unsigned char));
    memcpy(p->raw_packet, &recvline[0], p->raw_packet_length);
    p->raw_packet[p->raw_packet_length] = 0;

    return p;
}

