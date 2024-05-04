#ifndef TOPIC_H
#define TOPIC_H

#define TOPICS_SIZE 100
#define MAX_TOPIC_NAME_SIZE 129
#define TOPIC_MESSAGE_RETENTION_QUANTITY 100
#define MAX_MESSAGE_SIZE 129

#include "mqtt.h"

/**
 * Instead of creating a structure that would carry 1 name, the number of subscribers and the array of subscribers,
 * we are creating a structure that just carries a bunch of arrays.
 * The access to a certain topic will be done accessing the index.
 *
 * topic.current_offset[i], topic.messages[i], and topic.names[i] refer to the same topic.
 *
 * Basically, it makes it easier to create the arrays, i.e, we can malloc everything (the shared memory) before
 * start listening.
 */
typedef struct topic {
    int* current_offset;
    char** names;
    unsigned char*** messages;
    int** messages_length;
} topic;

extern topic topics;

void create_topic_structure();

void clean_topic_structure();

int create_topic(subscribe_packet* s);

int get_topic_id_by_name(char* topic);

int send_message(publish_packet* p);

#endif