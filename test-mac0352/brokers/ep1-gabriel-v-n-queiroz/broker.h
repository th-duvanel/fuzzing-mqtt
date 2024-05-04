#pragma once

#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <strings.h>
#include <signal.h>

#define WENT_OK 0
#define FAILED 1

typedef enum {
	HIGH = 2,
	MIDDLE = 1,
	LOW = 0,
} tristate;

void mqtt_error_print(char const function[], int line, char const fmt[], ...);
#define MQTT_ERROR(fmt,...) mqtt_error_print(__func__, __LINE__, fmt, ##__VA_ARGS__);
#define PRINT_HERE() printf("%s:%d ", __func__, __LINE__);

typedef struct {
	int fd;
	int pos;
	int len;
	size_t cap;
	bool closed;
	uint8_t *data;
} bio; // buffered io

bio *bio_new(int fd);
void bio_print(bio *buf);
void bio_ensure_capacity(bio *src, size_t new_capacity);
int  bio_preload(bio *src, int nbytes);
int  bio_read(bio *src, uint8_t *dst, int nbytes);
int  bio_read_wait(bio *src, uint8_t *dst, int nbytes);
uint8_t  bio_read_wait_u8(bio *src);
uint16_t bio_read_wait_u16(bio *src);
int  bio_write(bio *dst, uint8_t *src, int nbytes);
int  bio_write_u8(bio *src, uint8_t val);
int  bio_write_u16(bio *src, uint16_t val);
int  bio_flush_to(bio *buf, int fd);
void bio_clear(bio *buf);

typedef struct {
	bool username;
	bool password;
	bool will_retain;
	uint8_t will_qos;
	bool will_flag;
	bool clean_session;
	bool reserved;
	uint8_t keep_alive;
	char *client_id;
} mqtt_vheader_connect;

typedef struct {
	bool sp;
	uint8_t retcode;
} mqtt_vheader_connack;

typedef union {
	mqtt_vheader_connect connect;
	mqtt_vheader_connack connack;
	char *publish;
} mqtt_vheader;

typedef struct {
	bio *raw_data;
	uint8_t type;
	uint8_t flags;
	uint16_t identifier;
	mqtt_vheader header;
	bio *payload;
} mqtt_packet;

uint16_t mqtt_vle_decode(bio *input);
void mqtt_vle_encode(bio *output, uint16_t num);
void mqtt_packet_print(mqtt_packet *packet);
mqtt_packet *mqtt_packet_parse(bio *input);
mqtt_packet *mqtt_packet_new(uint8_t type, uint8_t flags, uint16_t packet_identifier);
mqtt_packet *mqtt_packet_new_connack(bool sp, uint8_t retcode);
mqtt_packet *mqtt_packet_new_unsuback(uint16_t packet_identifier);
mqtt_packet *mqtt_packet_new_suback(uint16_t packet_identifier, uint8_t qos);
mqtt_packet *mqtt_packet_new_publish(uint16_t packet_identifier, char const topic[], bio *msg);
mqtt_packet *mqtt_packet_new_pingresp();
bio *mqtt_packet_encode(mqtt_packet *packet);


typedef struct {
	int thread_id;
	int connfd;
	struct sockaddr *addr;
	socklen_t addr_len;
} BrokerConnArgs;

#define MAX_TOPICS 100
#define MAX_CONNECTIONS 1000
char *topics[MAX_TOPICS] = {NULL};
int   topics_n = 0;
int   topic2connfd[MAX_TOPICS][MAX_CONNECTIONS] = {0};
int   topic2connfd_n[MAX_TOPICS] = {0};
bool  connfd_enabled[MAX_CONNECTIONS] = {false};
pthread_mutex_t connfd_mutex[MAX_CONNECTIONS];
pthread_mutex_t global_mutex;
pthread_mutex_t print_mutex;
#define CRASH_IF_NOT_ZERO(x) if ((x) != 0) { exit(1); }

pthread_mutex_t packet_identifier_mutex;
uint16_t next_packet_identifier = 1;
uint16_t new_packet_identifier();

#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#define MIN(x, y) (((x) < (y)) ? (x) : (y))


#define MQTT_TYPE_RESERVED_1   0
#define MQTT_TYPE_CONNECT      1 // Client connection request to server
#define MQTT_TYPE_CONNACK      2 // Connection Acknowledgment
#define MQTT_TYPE_PUBLISH      3 // Publish Message
#define MQTT_TYPE_PUBBACK      4 // Publish Acknowledgment
#define MQTT_TYPE_PUBREC       5 //Publish recieved (assured delivery part 1)
#define MQTT_TYPE_PUBREL       6 //Publish release (assured delivery part 2)
#define MQTT_TYPE_PUBCOMP      7 //Publish complete (assured delivery part 3)
#define MQTT_TYPE_SUBSCRIBE    8
#define MQTT_TYPE_SUBACK       9
#define MQTT_TYPE_UNSUBSCRIBE 10
#define MQTT_TYPE_UNSUBACK    11
#define MQTT_TYPE_PINGREQ     12
#define MQTT_TYPE_PINGRESP    13
#define MQTT_TYPE_DISCONNECT  14 // Client is disconnecting
#define MQTT_TYPE_RESERVED_2  15

#define MQTT_FLAGS_RESERVED_1     0
#define MQTT_FLAGS_CONNECT        0
#define MQTT_FLAGS_CONNACK        0
// #define MQTT_FLAGS_PUBLISH      NOT DEFINED
#define MQTT_FLAGS_PUBLISH_DUP    8
#define MQTT_FLAGS_PUBLISH_QoS    6
#define MQTT_FLAGS_PUBLISH_RETAIN 1
#define MQTT_FLAGS_PUBBACK        0
#define MQTT_FLAGS_PUBREC         0
#define MQTT_FLAGS_PUBREL         2
#define MQTT_FLAGS_PUBCOMP        0
#define MQTT_FLAGS_SUBSCRIBE      2
#define MQTT_FLAGS_SUBACK         0
#define MQTT_FLAGS_UNSUBSCRIBE    2
#define MQTT_FLAGS_UNSUBACK       0
#define MQTT_FLAGS_PINGREQ        0
#define MQTT_FLAGS_PINGRESP       0
#define MQTT_FLAGS_DISCONNECT     0
#define MQTT_FLAGS_RESERVED_2     0