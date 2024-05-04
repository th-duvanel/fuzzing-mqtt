#include "mqtt.h"

DIR *dir;
struct dirent *ent;

/* Caminho base dos FIFOs */
const char base_path[13] = "/tmp/broker/";

int on_read(int connfd, unsigned char* recvline, int* fifofd, unsigned char* fifo_path) {
    unsigned char packet_type;
    unsigned char* packet;
    long offset = 0;
    long line_length = 0;
    long packet_length = 0;

    packet_type = parse_packet(connfd, recvline, &offset, &line_length);

    packet_length = line_length - offset;
    packet = malloc(packet_length * sizeof(char));
    memcpy(packet, &recvline[offset], packet_length);

    if(packet_type == CONNECT) {
        unsigned char connack[4] = {0x20, 0x02, 0x00, 0x00};
        debug_print("%s\n", "Received CONNECT");
        write(connfd, connack, 4);
        debug_print("%s\n", "Sent CONNACK");
        return CONNECT;
    } else if(packet_type == CONNACK) {
        debug_print("%s\n", "Received CONNACK");
        return CONNACK;
    } else if(packet_type == PUBLISH) {
        debug_print("%s\n", "Received PUBLISH");
        on_publish(recvline, packet, line_length);
        return PUBLISH;
    } else if(packet_type == SUBSCRIBE) {
        debug_print("%s\n", "Received SUBSCRIBE");
        *fifofd = on_subscribe(packet, fifo_path);
        debug_print("Subscriber FIFO: %s\n", fifo_path);
        unsigned char suback[5] = {0x90, 0x03, 0x00, 0x01, 0x00};
        write(connfd, suback, 5);
        debug_print("%s\n", "Received SUBACK");
        return SUBSCRIBE;
    }  else if(packet_type == PINGREQ) {
        debug_print("%s\n", "Received PINGREQ");
        unsigned char pingresp[2] = {0xd0, 0x00};
        write(connfd, pingresp, 2);
        debug_print("%s\n", "Sent PINGRESP");
        return PINGREQ;
    }  else if(packet_type == DISCONNECT) {
        debug_print("%s\n", "Received DISCONNECT");
        return DISCONNECT;
    }
    return -1;

}

/*
 * Le e faz o parse inical do pacote.
 * Entrada: o socket. 
 * Retona: o tipo do pacote. Coloca linha lida, o offset de inicio (2-5 bytes) e 
 * o tamanho do pacote nos parametros seguintes.
 */
int parse_packet(int connfd, unsigned char* recvline, long* offset, long* packet_length) {
    read(connfd, recvline, 1);
    unsigned char packet_type = (recvline[0] >> 4);
    long length = 0;

    for(int i = 1; i < 4; i++) {
        read(connfd, &recvline[i], 1);
        if((recvline[i] >> 8) & 1) {
            length = length << 7;
            length |= (recvline[i] ^ 128);
        } else {
            length |= (long) recvline[i];
            read(connfd, &recvline[++i], length);
            recvline[length+i] = '\0';
            *offset = i;

            length += i;
            *packet_length = length;
            break;
        }
    }

    return packet_type;
}

/*
 * Trata pacotes do tipo SUBSCRIBE criando um FIFO.
 * Entrada: o pacote sem o message type e remaining lenth em message.
 * Retona: o descritor do FIFO aberto. Coloca caminho do FIFO em fifo_path.
 */
int on_subscribe(unsigned char* message, unsigned char* fifo_path) {
    long name_length = (message[2] << 8) | message[3];
    unsigned char* topic_name = malloc((name_length+1) * sizeof(char));
    unsigned char random_suffix[7];

    /* Copy topic name from packet message*/
    strcpy((char*) topic_name, (char*) &message[4]);
    topic_name[name_length] = '\0';

    int topic_pipe_legth = 24 + name_length; 

    unsigned char* topic_pipe = malloc(topic_pipe_legth * sizeof(char));
    strcpy((char*) topic_pipe, base_path);
    strcat((char*) topic_pipe, (char*) topic_name);
    if(opendir((char*) topic_pipe) == NULL) {
        int ret = mkdir((char*) topic_pipe, 0666);
        if(ret == -1) perror("mkdir error: ");
        debug_print("Folder %s created\n", topic_pipe);
    }

    /* Get a random string to become pipe name*/
    rand_str(random_suffix, 6);
    strcat((char*) topic_pipe, "/");
    strcat((char*) topic_pipe, (char*) random_suffix);
    topic_pipe[topic_pipe_legth] = '\0';


    /* Copy fifo path to unlink later */
    memcpy(fifo_path, topic_pipe, topic_pipe_legth + 1);

    int fifofd;
    if((mkfifo((char*) topic_pipe, 0666)) == -1) {
        perror("mkfifo :(\n");
    } else debug_print("Pipe \"%s\" created\n", topic_pipe);

    fifofd = open((char*) topic_pipe, O_RDONLY | O_NONBLOCK);
    free(topic_name);
    free(topic_pipe);
    return fifofd;

}

/*
 * Trata pacotes do tipo PUBLISH, abrindo os FIFOS de todos os clientes do topico.
 * Entrada: a linha lida no socket, em recvline.
 * Retona: (int) 1 em caso de sucesso. Coloca o pacote sem o identificaodr
 * e o remaining length e tambem o tamanho do pacote em packet e line_length,
 * respectivamente.
 */
int on_publish(unsigned char* recvline, unsigned char* packet, long line_length) {
    long name_length = (packet[0] << 8) | packet[1];
    int base_path_len = strlen(base_path);
    unsigned char* topic_name = malloc((name_length+1) * sizeof(char));
    
    /* Copy topic name from packet message*/
    strcpy((char*) topic_name, (char*) &packet[2]);
    topic_name[name_length] = '\0';

    unsigned char* path = malloc((base_path_len + name_length + 8) * sizeof(char));
    strcpy((char*) path, base_path);
    strcat((char*) path, (char*) topic_name);
    strcat((char*) path, "/");
    path[(base_path_len + name_length + 2)] = '\0';

    if((dir = opendir((char*) path)) != NULL) {
        /* Caminho base passa a ser a base mais o nome do topico */
        base_path_len += name_length;
        /* Busca todos os arquivos dentro do diretorio
         * que contidos na pasta do fifo, se essa existir */
        while((ent = readdir(dir)) != NULL) {
            if(strcmp(ent->d_name, ".") == 0 ||
                strcmp(ent->d_name, "..") == 0) continue;
            else {
                strcpy((char*) &path[base_path_len+1], ent->d_name);
                int fd = open((char*) path, O_WRONLY | O_NONBLOCK);
                /*if(fd == -1){
                    perror("Open fifo error: ");
                }*/
                write(fd, " ", 1);
                write(fd, recvline, line_length);
                write(fd, "\n", 1);
                fsync(fd);
                close(fd);
            }
        }
    }
    free(topic_name);
    free(path);

    return 0;
}

/*
 * Gera uma string aleatoria com o charset.
 * Baseado na funcao disponivel em: 
 * https://stackoverflow.com/questions/15767691/whats-the-c-library-function-to-generate-random-string
 */
void rand_str(unsigned char *dest, size_t length) {
    srand(time(NULL));
    char charset[] = "abcdefghijklmnopqrstuvwxyz"
                     "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    while(length-- > 0) {
        size_t index = (double) rand() / RAND_MAX * (sizeof charset - 1);
        *dest++ = charset[index];
    }
    *dest = '\0';
}

void debug_print(const char* fmt, ...) {
    if(DEBUG == 1) {
        va_list args;
        va_start(args, fmt);
        printf("DEBUG: ");
        vprintf(fmt, args);
        va_end(args);
    }
}
