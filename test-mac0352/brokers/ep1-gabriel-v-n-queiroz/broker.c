#include "broker.h"

void mqtt_error_print(char const function[], int line, char const fmt[], ...) {
	va_list args;
	va_start(args, fmt);
	fprintf(stderr, "%s:%d \t\t", function, line);
	vfprintf(stderr, fmt, args);
	fprintf(stderr, "\n");
	va_end(args);
}

char* sockaddr2str(struct sockaddr *addr) {
	char *ip = calloc(sizeof(char), INET6_ADDRSTRLEN);
	char *out = calloc(sizeof(char), 128);
	
	if (addr == NULL) {
		sprintf(out, "<NULL ADDRESS>\n");
	} else if (addr->sa_family == AF_INET) {
		struct sockaddr_in *addr4 = (struct sockaddr_in *) addr;
		inet_ntop(AF_INET, &(addr4->sin_addr), ip, INET_ADDRSTRLEN);
		sprintf(out, "%s:%d", ip, addr4->sin_port);
	} else if (addr->sa_family == AF_INET6) {
		struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *) addr;
		inet_ntop(AF_INET, &(addr6->sin6_addr), ip, INET_ADDRSTRLEN);
		sprintf(out, "%s:%d", ip, addr6->sin6_port);
	} else {
		sprintf(out, "<address of family %d>\n", addr->sa_family);
	}
	return out;
}

void print_hex(uint8_t const ptr[], int len) {
	for (int i=0; i < len; i++) {
		printf("%02x ", ptr[i]);
	}
	printf("\n");
}

bio *bio_new(int fd) {
	bio *ans = calloc(sizeof(bio), 1);
	ans->fd = fd;
	return ans;
}

void bio_print(bio *buf) {
	// pthread_mutex_lock(&print_mutex);
	if (buf == NULL) {
		printf("bio at NULL.\n");
		// pthread_mutex_unlock(&print_mutex);
		return;
	}
	printf("bio at %p:\n", buf);
	printf("- fd: %d\n", buf->fd);
	printf("- pos: %d\n", buf->pos);
	printf("- len: %d\n", buf->len);
	printf("- cap: %ld\n", buf->cap);
	printf("- data: ");
	for (int i=0; i < buf->cap; i++) {
		if (i == buf->pos) {
			printf("[ ");
		}
		printf("%02x", buf->data[i]);
		if (i == buf->len) {
			printf("] ");
		} else {
			printf(" ");
		}
	}
	if (buf->cap == buf->pos) {
		printf("[ ");
	}
	if (buf->cap == buf->len) {
		printf("] ");
	}
	printf(".\n");
	// pthread_mutex_unlock(&print_mutex);
}

void bio_print_short(bio *buf) {
	// pthread_mutex_lock(&print_mutex);
	if (buf == NULL) {
		printf("bio at NULL.\n");
		// pthread_mutex_unlock(&print_mutex);
		return;
	}
	printf("bio at %p: >>> ", buf);
	for (int i=0; i < buf->cap; i++) {
		if (i == buf->pos) {
			printf("[ ");
		}
		if (i == buf->len) {
			printf("] ");
		}
		printf("%02x ", buf->data[i]);
	}
	if (buf->cap == buf->pos) {
		printf("[ ");
	}
	if (buf->cap == buf->len) {
		printf("] ");
	}
	printf(".\n");
	// pthread_mutex_unlock(&print_mutex);
}


void bio_ensure_capacity(bio *src, size_t new_capacity) {
	// PRINT_HERE() bio_print_short(src);
	if (src == NULL || src->cap > new_capacity) {
		return;
	}

	uint8_t *new_data = calloc(sizeof(uint8_t), new_capacity);
	memcpy(new_data, src->data, src->cap);
	free(src->data);
	src->data = new_data;
	src->cap = new_capacity;

	// PRINT_HERE() bio_print_short(src);
}

int  bio_preload(bio *src, int nbytes) {
	if (src == NULL || src->fd == 0 || nbytes <= 0) {
		return 0;
	}
	// PRINT_HERE() printf("nbytes = %d ", nbytes); bio_print_short(src);
	int n;
	bio_ensure_capacity(src, src->len+nbytes);
	// printf("Waiting for recv on %d\n", src->fd);
	n = recv(src->fd, &src->data[src->len], nbytes, 0);
	// printf("Finished recv on %d\n", src->fd);
	if (n < 0 || (n == 0 && errno != 0)) {
		char *tmp=calloc(sizeof(char), 100);
		sprintf(tmp, "%s:%d algo deu errado ao ler", __func__, __LINE__);
		perror(tmp);
		free(tmp);
		close(src->fd);
		pthread_exit(NULL);
	} else {
		src->len += n;
	}
	// PRINT_HERE() bio_print_short(src);
	return n;
}

int  bio_read(bio *src, uint8_t *dst, int nbytes) {
	if (src == NULL || nbytes <= 0) {
		return 0;
	}
	// PRINT_HERE() printf("nbytes = %d ", nbytes); bio_print_short(src);
	if (src->pos > src->len) {
		MQTT_ERROR("pos > len for bio = %p", src)
	}
	bio_preload(src, src->pos + nbytes - src->len);
	int n = MIN(nbytes, src->len - src->pos);
	// printf("%s:%d nbytes = %d src->len = %d src->pos = %d\n", __func__, __LINE__, nbytes, src->len, src->pos);
	// printf("%s:%d n = %d\n", __func__, __LINE__, n);
	memcpy(dst, &src->data[src->pos], n);
	src->pos += n;
	// printf("%s:%d bio = %p pos = %d len = %d\n", __func__, __LINE__, src, src->pos, src->len);
	// PRINT_HERE() bio_print_short(src);
	return n;
}

int  bio_read_wait(bio *src, uint8_t *dst, int nbytes) {
	if (src == NULL || nbytes <= 0) {
		return 0;
	}

	// PRINT_HERE() printf("nbytes = %d ", nbytes); bio_print_short(src);
	int tot = 0, rem = nbytes;
	bio_ensure_capacity(src, src->pos+rem);
	while (tot < nbytes) {
		int n = bio_read(src, &dst[tot], rem);
		rem -= n;
		tot += n;
	}
	// PRINT_HERE() bio_print_short(src);
	return tot;
}

uint8_t  bio_read_wait_u8(bio *src) {
	uint8_t ans;
	bio_read(src, &ans, 1);
	// printf("%s:%d read %d = 0x%x\n", __func__, __LINE__, ans, ans);
	return ans;
}

uint16_t bio_read_wait_u16(bio *src) {
	uint16_t encoded = 0;
	uint8_t *ptr = (uint8_t *) &encoded;
	bio_read_wait(src, ptr, 2);
	uint16_t ans = ntohs(encoded);
	// printf("%s:%d read %d = 0x%x\n", __func__, __LINE__, ans, ans);
	return ans;
}

int  bio_write(bio *dst, uint8_t *src, int nbytes) {
	if (src == NULL) {
		return 0;
	}
	bio_ensure_capacity(dst, dst->pos+nbytes);
	memcpy(&dst->data[dst->pos], src, nbytes);
	dst->pos += nbytes;
	dst->len += nbytes;
	return nbytes;
}


int  bio_write_u8(bio *src, uint8_t val) {
	return bio_write(src, &val, 1);
}

int  bio_write_u16(bio *src, uint16_t val) {
	uint16_t encoded = htons(val);
	return bio_write(src, (uint8_t *) &encoded, 2);
}

int  bio_flush_to(bio *buf, int fd) {
	if (buf == NULL || fd == 0) {
		return 0;
	}
	// PRINT_HERE() printf("flushing bio = %p to fd = %d\n", buf, fd);
	// bio_print_short(buf);
	int pos = 0;
	while (true) {
		// printf("Waiting for send on %d\n", fd);
		int n = send(fd, &buf->data[pos], buf->len-pos, MSG_NOSIGNAL);
		// printf("Finished send on %d (n=%d)\n", fd, n);
		if (n <= 0) {
			break;
		}
		pos += n;
		if (pos >= buf->len) {
			break;
		}
	}
	// PRINT_HERE() printf("flushed %d bytes from bio = %p to fd = %d\n", pos, buf, fd);
	return pos;
}

void bio_clear(bio *buf) {
	buf->len = 0;
	bzero(buf->data, buf->cap);
}

void mqtt_packet_print(mqtt_packet *packet) {
	MQTT_ERROR("not implemented yet")
}

uint16_t mqtt_vle_decode(bio *input) {
	int mult=1, val=0, cur=0;

	do {
		cur = bio_read_wait_u8(input);
		val += (cur & 127) * mult;
		mult *= 128;
		if (mult > 128*128*128) {
			MQTT_ERROR("Above 4 byte limit")
			return -1;
		}
	} while ((cur & 128) != 0);

	return val;
}

int mqtt_vle_needed(uint16_t val) {
	if (val <= 127) {
		return 1;
	} else if (val <= 16383) {
		return 2;
	} else if (val <= 2097151) {
		return 3;
	} else if (val <= 268435455) {
		return 4;
	} else {
		MQTT_ERROR("Above 4 byte limit")
		return -1;
	}
}

void mqtt_vle_encode(bio *output, uint16_t num) {
	if (num > 268435455) {
		MQTT_ERROR("Number %d is above 268435455 limit", num)
		return;
	}

	while (num > 0) {
		int cur = num % 128;
		num = num / 128;
		if (num > 0) {
			cur |= 128;
		}
		bio_write_u8(output, cur);
	}
}

mqtt_packet *mqtt_packet_parse(bio *input) {
	if (input == NULL) {
		return NULL;
	}

	bio_preload(input, 2);
	PRINT_HERE() bio_print_short(input);
	mqtt_packet *ans = mqtt_packet_new(0, 0, 0);
	int type_flags = bio_read_wait_u8(input);
	ans->type  = (type_flags & 0b11110000) >> 4; // Bits 7-4
	ans->flags = (type_flags & 0b00001111); // Bits 3-0

	// PRINT_HERE() bio_print_short(input);
	uint16_t rem_len=0, ref_pos=input->pos;
	rem_len = mqtt_vle_decode(input);
	// PRINT_HERE() printf("rem_len = %d\n", rem_len);
	// PRINT_HERE() bio_print_short(input);
	bio_preload(input, rem_len);
	// PRINT_HERE() bio_print_short(input);
	
	switch (ans->type) {
		case MQTT_TYPE_CONNECT:
			// MQTT_ERROR("got here")
			if (bio_read_wait_u8(input) != 0  ||
				bio_read_wait_u8(input) != 4  ||
				bio_read_wait_u8(input) != 'M' ||
				bio_read_wait_u8(input) != 'Q' ||
				bio_read_wait_u8(input) != 'T' ||
				bio_read_wait_u8(input) != 'T') {
				MQTT_ERROR("Expected first 6 bytes do not match")
				PRINT_HERE() bio_print_short(input);
				return NULL;
			}
			// MQTT_ERROR("got here")
			if (bio_read_wait_u8(input) != 4) {
				MQTT_ERROR("Wrong protocol level")
				return NULL;
			}
			// MQTT_ERROR("got here")

			int tmp = bio_read_wait_u8(input);
			// MQTT_ERROR("got here")
			ans->header.connect.username      = (tmp & 0b10000000) != 0;
			ans->header.connect.password      = (tmp & 0b01000000) != 0;
			ans->header.connect.will_retain   = (tmp & 0b00100000) != 0;
			ans->header.connect.will_qos      = (tmp & 0b00011000) << 3;
			ans->header.connect.will_flag     = (tmp & 0b00000100) != 0;
			ans->header.connect.clean_session = (tmp & 0b00000010) != 0;
			ans->header.connect.reserved      = (tmp & 0b00000001) != 0;
			if (ans->header.connect.reserved) {
				MQTT_ERROR("Reserved bit set to 1")
			}

			bio_print_short(input);
			ans->header.connect.keep_alive = bio_read_wait_u16(input);
			bio_print_short(input);
			// MQTT_ERROR("got here")

			// read client id
			int client_id_len = bio_read_wait_u16(input);
			if (client_id_len > 0) {
				ans->header.connect.client_id = calloc(sizeof(char), client_id_len+1);
				bio_read_wait(input, (uint8_t*) ans->header.connect.client_id, client_id_len);
			}
			bio_print_short(input);
			// MQTT_ERROR("got here")

			return ans;
		case MQTT_TYPE_SUBSCRIBE:
			ans->identifier = bio_read_wait_u16(input);
			char **payload_sub = calloc(sizeof(char*), MAX_TOPICS);
			int my_len = input->pos;
			for (int i=0; input->pos < ref_pos+rem_len && i < MAX_TOPICS-1; i++) {
				int topic_len = bio_read_wait_u16(input);
				payload_sub[i] = calloc(sizeof(char), topic_len+1);
				bio_read_wait(input, (uint8_t*) payload_sub[i], topic_len);
				bio_read_wait_u8(input); // get rid of the QoS request
			}
			my_len = input->pos - my_len;
			ans->payload = bio_new(0);
			ans->payload->cap = MAX_TOPICS*sizeof(char*);
			ans->payload->len = my_len;
			ans->payload->data = (uint8_t*) payload_sub;
			return ans;
		case MQTT_TYPE_UNSUBSCRIBE:
			bio_print_short(input);
			printf("MQTT_TYPE_UNSUBSCRIBE\n");
			ans->identifier = bio_read_wait_u16(input);
			int unsub_topic_len = bio_read_wait_u16(input);
			printf("unsub_topic_len = %d\n", unsub_topic_len);
			char **payload_unsub = calloc(sizeof(char**), MAX_TOPICS);
			payload_unsub[0] = calloc(sizeof(char*), unsub_topic_len+1);
			bio_read_wait(input, (uint8_t*) payload_unsub[0], unsub_topic_len);
			printf("payload_unsub[0] = %s\n", payload_unsub[0]);
			
			ans->payload = bio_new(0);
			ans->payload->cap = sizeof(char*)*MAX_TOPICS;
			ans->payload->len = ans->payload->cap;
			ans->payload->data = (uint8_t*) payload_unsub;
			bio_print_short(input);
			return ans;
		case MQTT_TYPE_PUBLISH:
			if ((ans->flags & 0x6) != 0) {
				ans->identifier = bio_read_wait_u16(input);
			}
			int topic_len = bio_read_wait_u16(input);
			ans->header.publish = calloc(sizeof(char), topic_len+1);
			bio_read_wait(input, (uint8_t*) ans->header.publish, topic_len);

			int msg_len = rem_len+ref_pos-input->pos+1;
			ans->payload = bio_new(0);
			ans->payload->cap = msg_len;
			ans->payload->len = msg_len;
			ans->payload->data = calloc(sizeof(char*), msg_len+1);
			bio_read_wait(input, ans->payload->data, msg_len);
		case MQTT_TYPE_PINGREQ:
		case MQTT_TYPE_PINGRESP:
		case MQTT_TYPE_DISCONNECT:
			// nothing to do
			return ans;
		default:
			MQTT_ERROR("Packet type not supported: %d", ans->type);
			PRINT_HERE() bio_print_short(input);
			// advance one byte so eventually we will (hopefully) get a correct packet
			bio_read_wait_u8(input);
			return ans;
	}
}

mqtt_packet *mqtt_packet_new(uint8_t type, uint8_t flags, uint16_t packet_identifier) {
	mqtt_packet *ans = calloc(sizeof(mqtt_packet), 1);
	ans->type = type;
	ans->flags = flags;
	ans->identifier = packet_identifier;
	return ans;
}

mqtt_packet *mqtt_packet_new_connack(bool sp, uint8_t retcode) {
	mqtt_packet *ans = mqtt_packet_new(MQTT_TYPE_CONNACK, MQTT_FLAGS_CONNACK, 0);
	ans->header.connack.sp = sp;
	ans->header.connack.retcode = retcode;
	return ans;
}

mqtt_packet *mqtt_packet_new_suback(uint16_t packet_identifier, uint8_t qos) {
	mqtt_packet *ans = mqtt_packet_new(MQTT_TYPE_SUBACK, MQTT_FLAGS_SUBACK, packet_identifier);
	ans->payload = bio_new(0);
	bio_write_u8(ans->payload, qos);
	return ans;	
}

mqtt_packet *mqtt_packet_new_unsuback(uint16_t packet_identifier) {
	mqtt_packet *ans = mqtt_packet_new(MQTT_TYPE_UNSUBACK, MQTT_FLAGS_UNSUBACK, packet_identifier);
	return ans;	
}

mqtt_packet *mqtt_packet_new_publish(uint16_t packet_identifier, char const topic[], bio *msg) {
	mqtt_packet *ans = mqtt_packet_new(MQTT_TYPE_PUBLISH, 0, packet_identifier);
	int len = strlen(topic);
	ans->header.publish = calloc(sizeof(char*), len+1);
	memcpy(ans->header.publish, topic, len);
	ans->payload = msg;
	return ans;
}

mqtt_packet *mqtt_packet_new_pingresp() {
	mqtt_packet *ans = mqtt_packet_new(MQTT_TYPE_PINGRESP, MQTT_FLAGS_PINGRESP, 0);
	return ans;	
}


bio *mqtt_packet_encode(mqtt_packet *packet) {
	if (packet == NULL) {
		return NULL;
	}

	int size_vheader = 0, size_packet_id = 0, size_payload = 0;
	if (packet->identifier != 0) {
		size_packet_id = 2;
	}
	switch (packet->type) {
		case MQTT_TYPE_CONNACK:
			size_vheader = 2;
			break;
		case MQTT_TYPE_PUBLISH:
			size_vheader = 2 + strlen(packet->header.publish);
			break;
		case MQTT_TYPE_SUBACK:
			size_vheader = 1;
		case MQTT_TYPE_UNSUBACK:
		case MQTT_TYPE_PINGRESP:
			size_vheader = 0;
			break;
		default:
			MQTT_ERROR("Packet type not supported: %d", packet->type);
			return NULL;
	}

	if (packet->payload != NULL) {
		size_payload = packet->payload->len;
	}

	// printf("size_vheader = %d\n", size_vheader);
	// printf("size_packet_id = %d\n", size_packet_id);
	// printf("size_payload = %d\n", size_payload);
	int rem_len = size_vheader + size_packet_id + size_payload;
	// printf("rem_len = %d\n", rem_len);
	int vle_len = mqtt_vle_needed(rem_len);
	// printf("vle_len = %d\n", vle_len);
	bio *output = bio_new(0);
	bio_ensure_capacity(output, rem_len+vle_len);

	bio_write_u8(output, packet->type << 4 | (packet->flags & 0b00001111));
	mqtt_vle_encode(output, rem_len);

	switch (packet->type) {
		case MQTT_TYPE_CONNACK:
			bio_write_u8(output, packet->header.connack.sp);
			bio_write_u8(output, packet->header.connack.retcode);
			break;
		case MQTT_TYPE_SUBACK:
		case MQTT_TYPE_UNSUBACK:
		case MQTT_TYPE_PINGRESP:
			// nothing to do
			break;
		case MQTT_TYPE_PUBLISH:
			int topic_len = strlen(packet->header.publish);
			bio_write_u16(output, topic_len);
			bio_write(output, (uint8_t*) packet->header.publish, topic_len);
			break;
		default:
			MQTT_ERROR("Packet type not supported: %d", packet->type);
			return NULL;
	}

	if (packet->identifier != 0) {
		bio_write_u16(output, packet->identifier);
	}

	if (packet->payload != NULL) {
		bio_write(output, packet->payload->data, packet->payload->len);
	}

	return output;
}

uint16_t new_packet_identifier() {
	pthread_mutex_lock(&packet_identifier_mutex);
	int ans = next_packet_identifier;
	next_packet_identifier++;
	pthread_mutex_unlock(&packet_identifier_mutex);
	return ans;
}

//  ----------------------

int find_topic(char const topic[]) {
	for (int i=0; i < topics_n; i++) {
		if (strcmp(topics[i], topic) == 0) {
			return i;
		}
	}
	return -1;
}

int add_topic(char const topic[]) {
	int ans = find_topic(topic);
	if (ans >= 0) {
		return ans;
	}
	pthread_mutex_lock(&global_mutex);
	topics[topics_n] = calloc(sizeof(char), strlen(topic)+1);
	strcpy(topics[topics_n], topic);
	ans = topics_n;
	topics_n += 1;
	pthread_mutex_unlock(&global_mutex);
	return ans;
}

void subscribe_topic(char const topic[], int connfd) {
	if (connfd == 0 || topic == NULL) {
		return;
	}
	int topic_id = add_topic(topic);
	pthread_mutex_lock(&global_mutex);
	bool already = false;
	for (int i=0; i < topic2connfd_n[topic_id]; i++) {
		if (topic2connfd[topic_id][i] == connfd) {
			PRINT_HERE() printf("Topic %s (%d) already sends to connfd %d\n", topic, topic_id, connfd);
			already = true;
			break;
		}
	}
	if (!already) {
		int i = topic2connfd_n[topic_id];
		topic2connfd[topic_id][i] = connfd;
		topic2connfd_n[topic_id]++;
		PRINT_HERE() printf("Topic %s (%d) now sends to connfd %d\n", topic, topic_id, connfd);
	}
	pthread_mutex_unlock(&global_mutex);
}

void unsubscribe_topic(char const topic[], int connfd) {
	if (connfd == 0 || topic == NULL) {
		return;
	}
	int topic_id = add_topic(topic);
	pthread_mutex_lock(&global_mutex);
	int pos = -1;
	for (int i=0; i < topic2connfd_n[topic_id]; i++) {
		if (topic2connfd[topic_id][i] == connfd) {
			pos = i;
			break;
		}
	}
	if (pos == -1) {
		PRINT_HERE() printf("Topic %s (%d) already doesn't send to connfd %d\n", topic, topic_id, connfd);
	} else {
		int last = topic2connfd_n[topic_id];
		if (pos == 0) {
			topic2connfd[topic_id][pos] = 0;
		} else {
			topic2connfd[topic_id][pos] = topic2connfd[topic_id][last-1];
		}
		topic2connfd_n[topic_id]--;
		PRINT_HERE() printf("Topic %s (%d) now doesn't send to connfd %d\n", topic, topic_id, connfd);
	}
	pthread_mutex_unlock(&global_mutex);
}

void publish_on_topic(char const topic[], bio *msg) {
	if (topic == NULL || msg == NULL) {
		return;
	}
	int topic_id = add_topic(topic);
	printf("%s:%d topic_id = %d\n", __func__, __LINE__, topic_id);

	for (int i=0; i < MAX_CONNECTIONS; i++) {
		printf("%s:%d Waiting for global mutex (i = %d)\n", __func__, __LINE__, i);
		pthread_mutex_lock(&global_mutex);
		int connfd = topic2connfd[topic_id][i];
		printf("%s:%d connfd = %d\n", __func__, __LINE__, connfd);
		if (connfd == 0) {
			pthread_mutex_unlock(&global_mutex);
			break;
		}
		bool enabled = connfd_enabled[connfd];
		pthread_mutex_unlock(&global_mutex);
		if (!enabled) {
			printf("%s:%d Skipping because connfd = %d is disabled\n", __func__, __LINE__, connfd);
			continue;
		}

		// make packet
		mqtt_packet *packet = mqtt_packet_new_publish(0, topic, msg);
		bio *data_out = mqtt_packet_encode(packet);

		// send it via the wire
		printf("%s:%d Waiting for mutex for connfd = %d\n", __func__, __LINE__, connfd);
		pthread_mutex_lock(&connfd_mutex[connfd]);
		printf("%s:%d Writing %d bytes on connfd = %d\n", __func__, __LINE__, data_out->len, connfd);
		print_hex(data_out->data, data_out->len);
		bio_flush_to(data_out, connfd);
		pthread_mutex_unlock(&connfd_mutex[connfd]);
		printf("%s:%d Released mutex for connfd = %d\n", __func__, __LINE__, connfd);

		free(data_out);
		free(packet);
	}
}

void broker_connection(BrokerConnArgs *args) {
	int thread_id = args->thread_id;
	int connfd = args->connfd;
	struct sockaddr *addr = args->addr;
	// socklen_t addr_len = args->addr_len;
	free(args);
	args = NULL;

	pthread_mutex_lock(&global_mutex);
	CRASH_IF_NOT_ZERO(pthread_mutex_init(&connfd_mutex[connfd], NULL));
	connfd_enabled[connfd] = true;
	pthread_mutex_unlock(&global_mutex);

	char *addr_txt = sockaddr2str(addr);
	printf("[Thread %d] Got connection from %s (descriptor = %d)\n", thread_id, addr_txt, connfd);
	free(addr_txt);

	
	bio *input = bio_new(connfd);
	bio *output = NULL;
	while (true) {
		printf("[Thread %d] Waiting\n", thread_id);
		mqtt_packet *packet = mqtt_packet_parse(input);
		printf("[Thread %d] Recieved: ", thread_id);
		mqtt_packet_print(packet);

		if (packet == NULL) {
			continue;
		}

		output = NULL;
		mqtt_packet *ans_packet = NULL;
		char **topics = NULL;
		switch (packet->type) {
			case MQTT_TYPE_CONNECT:
				printf("[Thread %d] Got connect\n", thread_id);
				ans_packet = mqtt_packet_new_connack(false, 0);
				printf("[Thread %d] Sending connack\n", thread_id);
				break;
			case MQTT_TYPE_SUBSCRIBE:
				printf("[Thread %d] Got subscribe\n", thread_id);
				topics = (char**) packet->payload->data;
				for (int i=0; i < MAX_TOPICS; i++) {
					subscribe_topic(topics[i], connfd);
				}
				printf("packet_id = %d\n", packet->identifier);
				ans_packet = mqtt_packet_new_suback(packet->identifier, 0);
				printf("packet_id = %d\n", ans_packet->identifier);
				printf("[Thread %d] Sending subback\n", thread_id);
				break;
			case MQTT_TYPE_UNSUBSCRIBE:
				printf("[Thread %d] Got unsubscribe\n", thread_id);
				topics = (char**) packet->payload->data;
				for (int i=0; i < MAX_TOPICS; i++) {
					unsubscribe_topic(topics[i], connfd);
				}
				ans_packet = mqtt_packet_new_unsuback(packet->identifier);
				printf("[Thread %d] Sending unsubback\n", thread_id);
				break;
			case MQTT_TYPE_PUBLISH:
				printf("[Thread %d] Got publish on topic '%s': \n", thread_id, packet->header.publish);
				bio *payload = packet->payload;
				publish_on_topic(packet->header.publish, payload);
				printf("[Thread %d] Finished sending message for topic '%s'\n", thread_id, packet->header.publish);
				break;
			case MQTT_TYPE_PINGREQ:
				printf("[Thread %d] Got ping request\n", thread_id);
				ans_packet = mqtt_packet_new_pingresp();
				printf("[Thread %d] Sending pingresp\n", thread_id);
				break;
			case MQTT_TYPE_DISCONNECT:
				printf("[Thread %d] Got disconnect\n", thread_id);
				break;
			default:
				printf("[Thread %d] Unsupported packet type: %d\n", thread_id, packet->type);
				sleep(3); // reduces pointless messages
				break;
		}
		if (packet->type == MQTT_TYPE_DISCONNECT) {
			break;
		}
		if (ans_packet != NULL) {
			output = mqtt_packet_encode(ans_packet);
		}
		if (output != NULL) {
			pthread_mutex_lock(&connfd_mutex[connfd]);
			printf("[Thread %d] Sending: ", thread_id);
			bio_print_short(output);
			bio_flush_to(output, connfd);
			pthread_mutex_unlock(&connfd_mutex[connfd]);
			free(output);
		}

		// sleep(2);
	}
	printf("[Thread %d] Closing\n", thread_id);

	pthread_mutex_lock(&global_mutex);
	connfd_enabled[connfd] = false;
	pthread_mutex_unlock(&global_mutex);
	pthread_mutex_lock(&connfd_mutex[connfd]);
	close(connfd);
	pthread_mutex_destroy(&connfd_mutex[connfd]);
	printf("[Thread %d] Closed\n", thread_id);

	free(addr);
}

int broker_run(uint16_t port) {
	int listenfd, connfd, threads=1;
	struct sockaddr_in servaddr;

	CRASH_IF_NOT_ZERO(pthread_mutex_init(&global_mutex, NULL));
	CRASH_IF_NOT_ZERO(pthread_mutex_init(&packet_identifier_mutex, NULL));

	if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Failed to open socket");
		return 2;
	}

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family      = AF_INET;
	servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	servaddr.sin_port        = htons(port);
	if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
		perror("Failed to bind socker");
		return 3;
	}

	if (listen(listenfd, 0) == -1) {
		perror("Failed to listen on socket");
		return 4;
	}

	printf("[Server listening on 0.0.0.0:%d]\n", port);

	while (true) {
		socklen_t addr_len = 128;
		struct sockaddr *addr = calloc(sizeof(uint8_t), addr_len);
		connfd = accept(listenfd, addr, &addr_len);
		if (connfd == -1) {
			perror("Failed to accept connection");
		}
		
		BrokerConnArgs *args = calloc(sizeof(BrokerConnArgs), 1);
		args->thread_id = threads++;
		args->connfd = connfd;
		args->addr = addr;
		args->addr_len = addr_len;
		pthread_t *thread = calloc(sizeof(pthread_t), 1);
		pthread_create(thread, NULL, (void * (*)(void *)) broker_connection, args);
	}

	printf("[Server stopping]\n");

	return 0;
}

int main(int argc, char const *argv[]) {
	signal(SIGPIPE, SIG_IGN);

	uint16_t port = 1883;
	if (argc > 2) {
		fprintf(stderr,"Use: %s [port]\n", argv[0]);
		exit(1);
	}
	if (argc == 2) {
		port = atoi(argv[1]);
		if (port == 0) {
			fprintf(stderr,"%s is not an integer\n", argv[1]);
			exit(1);
		}
	}
	return broker_run(port);
}