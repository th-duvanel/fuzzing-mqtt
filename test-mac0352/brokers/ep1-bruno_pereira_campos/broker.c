/**
 * EP1 - Redes de Computadores
 * Nome: Bruno Pereira Campos
 * Nusp: 11806764
 * Descrição: Um broker MQTT
 * OBS: Foi utilizado uma biblioteca externa pertencente
 * ao usuário Zunawe no github.
 * Link do projeto: https://github.com/Zunawe/md5-c
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "md5.h"
#include <dirent.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096
#define MD5HASH_LENGTH 16

enum packet_types
{
    CONNECT = 0x01,
    CONNACK = 0x02,
    PUBLISH = 0x03,
    PUBACK = 0x04,
    SUBSCRIBE = 0x08,
    SUBACK = 0x09,
    PINGREQ = 0x0c,
    PINGRESP = 0x0d,
    DISCONNECT = 0x0e
};

/**
 * Função: recebe o buffer o um offset e salva em
 * remaning_length e remaning_length_size, respectivamente
 * o valor do campo remaning_length e a quantidade de bytes
 * que ele ocupa.
 **/
void get_remaning_length(unsigned char *buffer, unsigned int buffer_offset, unsigned int *remaning_length, unsigned int *remaning_length_size)
{
    unsigned char limit = 0x80; // 128
    unsigned int sum = 0x00;
    unsigned int rounds = 0;
    do
    {
        unsigned int aux = (unsigned int)buffer[buffer_offset + rounds];
        if (aux >= limit)
            aux -= limit;
        aux = aux << rounds * 7;
        sum += aux;
        rounds++;
    } while (buffer[buffer_offset + rounds - 1] >= limit);
    *remaning_length = sum;
    *remaning_length_size = rounds;
}

/**
 * Função: recebe o buffer e um offset e salva em
 * topic_length o valor do campo do pacote.
 **/
void get_topic_length(unsigned char *buffer, unsigned int buffer_offset, unsigned int *topic_length)
{
    unsigned int msb_byte = (unsigned int)buffer[buffer_offset];
    msb_byte = msb_byte << 8;
    unsigned int t_length = msb_byte + buffer[buffer_offset + 1];
    *topic_length = t_length;
}

/**
 * Função: recebe um hash e retorna a string
 * da impressão hexadecimal desse hash.
 **/
char *topic_hash_to_str(unsigned char *hash)
{
    char *str = malloc(sizeof(char) * MD5HASH_LENGTH * 2 + 1);
    char aux[3];
    for (int i = 0; i < MD5HASH_LENGTH; i++)
    {
        snprintf(aux, 3, "%02x", hash[i]);
        strcat(str, aux);
    }
    return str;
}

/**
 * Função: recebe uma path base e o nome do diretório
 * e cria esse diretório temporário.
 **/
int create_tmp_dir(char *base_dir_path, char *dir_name)
{
    char *path = malloc(sizeof(char) * (12 + MD5HASH_LENGTH));
    strcpy(path, base_dir_path);
    strcat(path, "/");
    strcat(path, dir_name);
    if (mkdir(path, S_IRWXU) == -1)
    {
        if (errno == EEXIST)
        {
            return 1;
        }
        perror("mkdir :(\n");
        exit(6);
        return -1;
    }
    free(path);
    return 0;
}

/**
 * Função: recebe uma path string, uma path para
 * o diretório base, uma path para o diretório do
 * aqruivo e salva na path string o template para
 * criação do arquivo temporário via mkstemp
 **/
void save_file_path(char *path, char *base_dir_path, char *file_dir)
{
    strcpy(path, base_dir_path);
    strcat(path, "/");
    char post_file_template[8] = {'/', 'X', 'X', 'X', 'X', 'X', 'X', '\0'};
    strcat(path, file_dir);
    strcat(path, post_file_template);
}

int main(int argc, char **argv)
{
    /* Os sockets. Um que será o socket que vai escutar pelas conexões
     * e o outro que vai ser o socket específico de cada conexão */
    int listenfd, connfd;
    /* Informações sobre o socket (endereço e porta) ficam nesta struct */
    struct sockaddr_in servaddr;
    /* Retorno da função fork para saber quem é o processo filho e
     * quem é o processo pai */
    pid_t childpid;
    /* Armazena linhas recebidas do cliente */
    unsigned char recvline[MAXLINE + 1];
    /* Armazena o tamanho da string lida do cliente */
    ssize_t n;

    /* Identificador do tipo de pacote */
    unsigned char packet_type_code;

    /* Bytes do header connack */
    unsigned char connack_header[4] = {0x20, 0x02, 0x00, 0x00};

    /* Bytes do header suback */
    unsigned char suback_header[5] = {0x90, 0x03, 0x00, 0x01, 0x00};

    /* Bytes do header pingresp */
    unsigned char pingresp_header[2] = {0xd0, 0x00};

    /* Hash do nome do topico */
    unsigned char *topic_hash;

    /* nome do topico */
    unsigned char topic_name[MAXLINE + 1];

    /* String do hash do nome topico */
    char *topic_hash_str;

    /* String da path de arquivos */
    char file_path[60];

    /* Tamanho restante do pacote */
    unsigned int remaning_length;

    /* Tamanho do campo remaning length */
    unsigned int remaning_length_size;

    /* Tamaho do topico */
    unsigned int topic_length;

    /* Posicao de leitura do buffer */
    unsigned int buffer_offset;

    /* controle se foi inscrito */
    int has_subscribed = 0;

    /* controle da disconexao */
    int disconnect = 0;

    /* Path do diretório onde ficarão todos os arquivos do programa */
    char base_path[12] = {'/', 't', 'm', 'p', '/', 'X', 'X', 'X', 'X', 'X', 'X', '\0'};

    // gera o nome do diretório base
    mkdtemp(base_path);
    printf("Iniciando\nGerado diretório em %s\n", base_path);

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <Porta>\n", argv[0]);
        fprintf(stderr, "Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket :(\n");
        exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1)
    {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n", argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

    for (;;)
    {
        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
        {
            perror("accept :(\n");
            exit(5);
        }
        if ((childpid = fork()) == 0)
        {
            close(listenfd);
            memset(recvline, 0, MAXLINE);
            while ((n = read(connfd, recvline, MAXLINE)) > 0)
            {
                // Distinção entre pacotes
                packet_type_code = recvline[0] >> 4;
                buffer_offset = 1;
                switch (packet_type_code)
                {
                case CONNECT:
                    printf("Connect Client\n");
                    write(connfd, connack_header, 4);
                    break;
                case SUBSCRIBE:
                    printf("Subscribe Client\n");
                    get_remaning_length(recvline, buffer_offset, &remaning_length, &remaning_length_size);
                    buffer_offset += remaning_length_size;
                    // pula message identifier
                    buffer_offset += 2;
                    get_topic_length(recvline, buffer_offset, &topic_length);
                    buffer_offset += 2;
                    memcpy(topic_name, &recvline[buffer_offset], topic_length);
                    // cria hash com topico
                    printf("Se increveu em: %s\n", topic_name);
                    topic_hash = md5String((char *)topic_name);
                    topic_hash_str = topic_hash_to_str(topic_hash);
                    // cria path do pipe
                    create_tmp_dir(base_path, topic_hash_str);
                    save_file_path(file_path, base_path, topic_hash_str);
                    // cria pipe
                    if (mkstemp(file_path) == -1)
                    {
                        perror("mkstemp :(");
                    }
                    remove(file_path);
                    if (mkfifo((const char *)file_path, 0644) == -1)
                    {
                        perror("mkfifo :(\n");
                        exit(7);
                    }

                    if ((childpid == fork()) == 0)
                    {
                        int pipe_fd = open(file_path, O_RDWR);
                        memset(recvline, 0, MAXLINE);
                        while ((n = read(pipe_fd, recvline, MAXLINE)) > 0)
                        {
                            write(connfd, recvline, n);
                            memset(recvline, 0, MAXLINE);
                        }
                        close(connfd);
                        close(pipe_fd);
                        exit(0);
                    }
                    // manda o buffer suback
                    write(connfd, suback_header, 5);

                    // controle inscrito
                    has_subscribed = 1;
                    break;
                case PINGREQ:
                    printf("Client Ping\n");
                    write(connfd, pingresp_header, 2);
                    break;
                case PUBLISH:
                    printf("Publish Client\n");
                    // calcula campos remaning_length e topic length
                    get_remaning_length(recvline, buffer_offset, &remaning_length, &remaning_length_size);
                    buffer_offset += remaning_length_size;
                    get_topic_length(recvline, buffer_offset, &topic_length);
                    buffer_offset += 2;

                    memcpy(topic_name, &recvline[buffer_offset], topic_length);
                    printf("Publicou no tópico: %s\n", topic_name);

                    // cria hash com topico
                    topic_hash = md5String((char *)topic_name);
                    topic_hash_str = topic_hash_to_str(topic_hash);

                    // salva o diretorio do topico na file_path
                    save_file_path(file_path, base_path, topic_hash_str);
                    file_path[44] = '\0';

                    struct dirent *files;
                    DIR *dir = opendir(file_path);
                    if (dir != NULL)
                    {
                        while ((files = readdir(dir)) != NULL)
                        {
                            char *f_name = files->d_name;
                            if (strcmp(f_name, ".") != 0 && strcmp(f_name, "..") != 0)
                            {
                                strcat(file_path, "/");
                                strcat(file_path, f_name);
                                int pipe_fd = open(file_path, O_WRONLY);
                                write(pipe_fd, recvline, 1 + remaning_length_size + remaning_length);
                                file_path[44] = '\0';
                                close(pipe_fd);
                            }
                        }
                    }
                    else
                        printf("Cannot be open\n");
                    closedir(dir);
                    break;
                case DISCONNECT:
                    printf("Disconnect Client\n");
                    if (has_subscribed)
                        remove(file_path);
                    disconnect = 1;
                    break;
                default:
                    printf("Unidentified Client\n");
                    break;
                }
                // zera buffer
                memset(recvline, 0, MAXLINE);

                // disconectou
                if (disconnect)
                    break;
            }

            printf("[Uma conexão fechada]\n");
            close(connfd);
            exit(0);
        }
        else
            close(connfd);
    }
    exit(0);
}