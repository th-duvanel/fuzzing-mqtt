#define _GNU_SOURCE

#include <stdlib.h>
#include <stdio.h>
/** Para usar o mkfifo() **/
#include <sys/types.h>
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <ftw.h>
#include "hashmap.h"
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

char *packMsg(char *msg, char *topic);
void make_pipe(char fileTemplate[]);
typedef struct subscriber
{
    int fifo;
    struct subscriber *next;

} Subscriber_t;

Subscriber_t *new_node()
{
    Subscriber_t *new = malloc(sizeof(struct subscriber));
    new->fifo = -1;
    new->next = NULL;
    return new;
}

typedef struct listaligada
{
    struct subscriber *first;
    struct subscriber *last;
    char *topic;
    /* data */
} Listaligada;

Listaligada *criaLista()
{
    Listaligada *lista = malloc(sizeof(struct listaligada));
    lista->first = NULL;
    lista->last = NULL;
    lista->topic = malloc(sizeof(char) * 256);
    return lista;
}

int lista_compare(const void *a, const void *b, void *udata)
{
    const struct listaligada *la = a;
    const struct listaligada *lb = b;
    return strcmp(la->topic, lb->topic);
}

uint64_t lista_hash(const void *item, uint64_t seed0, uint64_t seed1)
{
    const struct listaligada *lista = item;
    return hashmap_sip(lista->topic, strlen(lista->topic), seed0, seed1);
}

void handle_subscriber(char *pipeName, struct hashmap *topics, Listaligada *lista, char *topic)
{
    int tmpFd;
    Subscriber_t *new;
    tmpFd = open(pipeName, O_WRONLY);

    // insere o fd no hashmap. talvez seja bom salvar
    //  a string, e abrir e fechar o pipe.
    new = new_node();
    new->fifo = tmpFd;
    if (lista)
    {
        lista->last->next = new;
        lista->last = new;
    }
    else
    {
        lista = criaLista();
        strcpy(lista->topic, topic);
        lista->first = new;
        lista->last = new;
        hashmap_set(topics, lista);
    }
}

void handle_publisher(char *pipeName, char *buffer, Listaligada *lista)
{
    int tmpFd;
    Subscriber_t *new;
    tmpFd = open(pipeName, O_RDONLY);
    read(tmpFd, buffer, 1024);
    if (lista)
    {
        for (new = lista->first; new != NULL; new = new->next)
        {
            write(new->fifo, buffer, 1024);
        }
    }
    unlink(pipeName);
    close(tmpFd);
}

int get_pipe(int ctrlPipeFd, char *topic, char sub)
{
    int tmpPipeFd;
    char *fileTemplate = "tmpPipe.XXXXXX";
    char tmpPipe[15];
    char *sufix;
    char message[270];

    strcpy(tmpPipe, fileTemplate);
    make_pipe(tmpPipe);
    sufix = &tmpPipe[8];
    sprintf(message, "%s %s", sufix, topic);
    message[6] = sub;
    write(ctrlPipeFd, message, 270);

    if (sub == '1')
    {
        tmpPipeFd = open(tmpPipe, O_RDONLY);
    }
    else
    {
        tmpPipeFd = open(tmpPipe, O_WRONLY);
    }
    unlink(tmpPipe);
    return tmpPipeFd;
}

void send_pipe(char *recvline, struct hashmap *topics)
{
    char *topic;
    char tmpPipe[15];
    int sub;
    Listaligada *lista;
    Listaligada *tmpList;

    if (recvline[6] == '1')
        sub = 1;
    else
        sub = 0;

    recvline[6] = '\0';
    topic = &recvline[7];

    sprintf(tmpPipe, "tmpPipe.%s", recvline);

    tmpList = criaLista();
    strcpy(tmpList->topic, topic);
    lista = hashmap_get(topics, tmpList);

    if (sub == 1)
    {
        handle_subscriber(tmpPipe, topics, lista, topic);
    }
    else
    {
        handle_publisher(tmpPipe, recvline, lista);
    }
}

void run_manager(char *ctrlPipe,
                 char *buffer,
                 struct hashmap *topics)
{
    int ctrlPipeFd;
    ctrlPipeFd = open(ctrlPipe, O_RDONLY);
    if (ctrlPipeFd == -1)
    {
        perror("Error opening RDWR FIFO");
        exit(0);
    }

    for (;;)
    {
        read(ctrlPipeFd, buffer, 270);
        send_pipe(buffer, topics);
    }
    unlink(ctrlPipe);
    close(ctrlPipeFd);
}

void run_subscriber(char *ctrlPipe, char *topic, int connFd)
{
    int clientFd;
    int ctrlPipeFd;
    int n;
    char *recvline = malloc(sizeof(char) * 1025);
    char *msg;

    // Abre o pipe de controle para se comunicar com
    // o processo central. O mesmo para todos os
    // processos.
    ctrlPipeFd = open(ctrlPipe, O_WRONLY);
    if (ctrlPipeFd == -1)
    {
        perror("Error opening RDONLY FIFO");
        exit(0);
    }
    // Se comunica com o processo central para receber
    // o nome do pipe específico deste cliente.
    clientFd = get_pipe(ctrlPipeFd, topic, '1');
    while (1)
    {
        n = read(clientFd, recvline, 1024);
        if (n > 0)
        {
            recvline[n] = 0;
            msg = packMsg(recvline, topic);
            write(connFd, msg, strlen(recvline) + strlen(topic) + 4);
        }
    }
    close(ctrlPipeFd);
}

void run_publisher(char *ctrlPipe, char *topic, char *message)
{
    int ctrlPipeFd;
    int clientFd;
    // Abre o pipe de controle para se comunicar com
    // o processo central. O mesmo para todos os
    // processos.
    ctrlPipeFd = open(ctrlPipe, O_WRONLY);
    if (ctrlPipeFd == -1)
    {
        perror("Error opening RDONLY FIFO");
        exit(0);
    }
    // Se comunica com o processo central para receber
    // o nome do pipe específico deste cliente.
    clientFd = get_pipe(ctrlPipeFd, topic, '0');
    // Escreve a mesnagem no pipe deste cliente.
    write(clientFd, message, 256);
    close(clientFd);
    close(ctrlPipeFd);
}

void make_pipe(char fileTemplate[])
{
    int tmpfile;
    int s;
    int errnum;
    tmpfile = mkstemp(fileTemplate);

    if (!tmpfile)
    {
        errnum = errno;
        fprintf(stderr, "Value of errno: %d\n", errno);
        perror("Error printed by perror");
    }
    unlink(fileTemplate);

    s = mkfifo((const char *)fileTemplate, 0644);
    if (s == -1)
    {
        errnum = errno;
        fprintf(stderr, "Error opening fifo: %s\n", strerror(errnum));
    }
}

int handle_conn()
{
    return 0;
}

int send_connack(int connfd)
{
    unsigned char response[4] = {0x20, 0x02, 0x00, 0x00};
    write(connfd, response, (unsigned int)4);
    return 0;
}

int send_suback(int connfd)
{
    unsigned char response[5] = {0x90, 0x03, 0x00, 0x01, 0x00};
    write(connfd, response, (unsigned int)5);
    return 0;
}

char *get_topic(char *payload)
{
    unsigned char length = payload[1];
    int len = (int)length;
    char *topic = malloc(sizeof(char) * 256);
    strncpy(topic, payload + 2, len);
    topic[len] = '\0';
    return topic;
}
char *get_message(char *payload, int topicLenght, int rlen)
{
    char *message = malloc(sizeof(char) * 256);
    strncpy(message, payload + 4 + topicLenght, 256);
    message[rlen - topicLenght - 2] = '\0';
    return message;
}

char *packMsg(char *msg, char *topic)
{
    unsigned int msglen = strlen(msg) + strlen(topic) + 2;
    unsigned int totallen = msglen + 2;
    char *packet = malloc(sizeof(char) * 258);
    packet[0] = 0x30;
    packet[1] = (char)msglen;
    packet[2] = '\0';
    packet[3] = (char)strlen(topic);
    strcpy(&packet[4], topic);
    strcpy(&packet[4 + strlen(topic)], msg);
    packet[totallen] = '\0';
    return packet;
}

int main(int argc, char **argv)
{
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    pid_t childpid;
    char recvline[MAXLINE + 1];
    ssize_t n;

    char *topic;
    char *msg;
    int rlen;

    char fifoTemplate[] = "dXXXXXX";
    char ctrlPipe[8];

    int cliente;
    cliente = -2;
    struct hashmap *topics = hashmap_new(sizeof(struct listaligada), 0, 0, 0,
                                         lista_hash, lista_compare, NULL, NULL);

    /*********************************************************************
     *        CONFIGURAÇÂO DE PORTAS E SOCKETS. IGUAL AO ORIGINAL
     * ******************************************************************/

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <Porta>\n", argv[0]);
        fprintf(stderr, "Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    strcpy(ctrlPipe, fifoTemplate);
    make_pipe(ctrlPipe);

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

    /********************************************************************
     *
     *          FINAL CONFIRAÇÂO PORTAS E SOCKETS.
     *
     * *****************************************************************/

    if ((childpid = fork()) != 0)
    {
        run_manager(ctrlPipe, recvline, topics);
    }
    else
    {
        for (;;)
        {
            if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
            {
                perror("accept :(\n");
                exit(5);
            }

            cliente++;

            if ((childpid = fork()) == 0)
            {
                /**** PROCESSO FILHO ****/
                printf("[Uma conexão aberta]\n");
                close(listenfd);

                /* ========================================================= */
                /* ========================================================= */
                /*                         Começo do read connfd             */
                /* ========================================================= */
                /* ========================================================= */

                while ((n = read(connfd, recvline, MAXLINE)) > 0)
                {
                    recvline[n] = 0;
                    printf("[Cliente conectado no processo filho %d enviou:]\n ", getpid());
                    if ((fputs(recvline, stdout)) == EOF)
                    {
                        perror("fputs :( \n");
                        exit(6);
                    }
                    unsigned char code = recvline[0];

                    if (code == 0x10)
                    {
                        send_connack(connfd);
                    }
                    else if (code == 0x82)
                    {
                        topic = get_topic(&recvline[4]);
                        send_suback(connfd);
                        run_subscriber(ctrlPipe, topic, connfd);
                    }
                    else if (code == 0x30)
                    {
                        send_suback(connfd);
                        topic = get_topic(&recvline[2]);
                        rlen = (int)recvline[1];
                        msg = get_message(recvline, strlen(topic), rlen);
                        run_publisher(ctrlPipe, topic, msg);
                    }
                }

                /* ========================================================= */
                /* ========================================================= */
                /*                         EP1 FIM                           */
                /* ========================================================= */
                /* ========================================================= */

                printf("[Uma conexão fechada]\n");
                exit(0);
            }
            else
                close(connfd);
        }
    }

    exit(0);
}
