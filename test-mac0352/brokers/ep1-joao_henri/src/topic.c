#include "topic.h"

#include <sys/mman.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MESSAGE_SIZE 129


/**
 * Uses mmap to allocate memory space that is shared across all child processes of this broker.
 *
 * Below, the explanation for each flag passed to mmap:
 * 1) NULL: the kernel can place the mapping anywhere it sees fit.
 * 2) size: the number of bytes to be allocated
 * 3) PROT_READ | PROT_WRITE: processes have RW permission on the contents of this array
 * 4) MAP_SHARED: All processes related to this array can access it. MAP_ANONYMOUS: Any other processes
 * have no access to it and it won't be written to any file.
 * 5) File descriptor to be mapped. As we are using MAP_ANONYMOUS, the file descriptor is set to 0, i.e,
 * it won't be written to any specific file descriptor.
 * 6) Offset value. As there is no file descriptor, it will be 0 as well.
 *
 * Special thanks to https://linuxhint.com/using_mmap_function_linux/
 */
void* malloc_shared_memory(size_t size) {
    void* memory = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, 0, 0);
    if (memory == MAP_FAILED) {
        fprintf(stderr, "Couldn't malloc shared memory.\n");
        exit(errno);
    }

    return memory;
}

void free_shared_memory(void* ptr, size_t size) {
    int err = munmap(ptr, size);
    if (err == -1) {
        fprintf(stderr, "Couldn't free shared memory.\n");
        exit(errno);
    }
}

topic topics;

void create_topic_structure() {
    topics.names           = malloc_shared_memory(TOPICS_SIZE * sizeof(char*));
    topics.messages        = malloc_shared_memory(TOPICS_SIZE * sizeof(unsigned char**));
    topics.messages_length = malloc_shared_memory(TOPICS_SIZE * sizeof(int*));
    topics.current_offset  = malloc_shared_memory(TOPICS_SIZE * sizeof(int));

    for (int i = 0; i < TOPICS_SIZE; i++) {
        topics.names[i]    = malloc_shared_memory(MAX_TOPIC_NAME_SIZE * sizeof(char));
        topics.names[i][0] = 0;

        topics.messages[i] = malloc_shared_memory(TOPIC_MESSAGE_RETENTION_QUANTITY * sizeof(unsigned char*));
        topics.messages_length[i] = malloc_shared_memory(TOPIC_MESSAGE_RETENTION_QUANTITY * sizeof(int));

        for (int j = 0; j < TOPIC_MESSAGE_RETENTION_QUANTITY; j++) {
            topics.messages[i][j] = malloc_shared_memory(MAX_MESSAGE_SIZE * sizeof(unsigned char));
            topics.messages[i][j][0] = 0;
        }

        topics.current_offset[i] = -1;
    }
}

void clean_topic_structure() {
    for (int i = 0; i < TOPICS_SIZE; i++) {
        for (int j = 0; j < TOPIC_MESSAGE_RETENTION_QUANTITY; j++) {
            free_shared_memory(topics.messages[i][j], MAX_MESSAGE_SIZE * sizeof(unsigned char));
        }

        free_shared_memory(topics.messages[i], TOPIC_MESSAGE_RETENTION_QUANTITY * sizeof(unsigned char*));
        free_shared_memory(topics.messages_length, TOPIC_MESSAGE_RETENTION_QUANTITY * sizeof(int));
        free_shared_memory(topics.names, MAX_TOPIC_NAME_SIZE * sizeof(char));

    }

    free_shared_memory(topics.names, TOPICS_SIZE * sizeof(char*));
    free_shared_memory(topics.messages, TOPICS_SIZE * sizeof(unsigned char**));
    free_shared_memory(topics.messages_length, TOPICS_SIZE * sizeof(int*));
    free_shared_memory(topics.current_offset, TOPICS_SIZE * sizeof(int));
}

int get_topic_id_by_name(char* topic) {
    for (int i = 0; i < TOPICS_SIZE; i++) {
        if (strcmp(topic, topics.names[i]) == 0) {
            return i;
        }
    }

    return -1;
}

int create_topic(subscribe_packet* s) {
    for (int i = 0; i < TOPICS_SIZE; i++) {
        if (topics.names[i][0] == 0) {
            memcpy(topics.names[i], s->topic, s->topic_length);
            topics.names[i][s->topic_length] = 0;
            return i;
        }
    }

    return -1;
}

int send_message(publish_packet* p) {
    int topic_id = get_topic_id_by_name(p->topic);

    if (topic_id == -1) {
        return -1;
    }

    int new_offset = (topics.current_offset[topic_id] + 1) % TOPIC_MESSAGE_RETENTION_QUANTITY;

    memcpy(topics.messages[topic_id][new_offset], p->raw_packet, p->raw_packet_length);
    topics.messages[topic_id][new_offset][p->raw_packet_length] = 0;
    topics.current_offset[topic_id] = new_offset;
    topics.messages_length[topic_id][new_offset] = p->raw_packet_length;

    printf("Topic ID %d new offset is %d and packet size is %d\n", topic_id, topics.current_offset[topic_id], topics.messages_length[topic_id][new_offset]);
    return 0;
}