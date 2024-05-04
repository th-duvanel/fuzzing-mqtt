/* Por Luca Assumpção Dillenburg
 * Em 16/3/2022
 *
 * Servidor broker que implementa as funções básicas do MQTT.
 * Baseado em um servidor de eco disponibilizado pelo Prof. Daniel Batista
 * <batista@ime.usp.br>.
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "data.c"
#include "utils.c"
#include "files.c"

#define SHOW_LOG 0

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096
#define BROKER_PORT "1883"

/* Function Headers */
void start_broker_server();
void process_mqtt(int connfd);

void read_fixed_header(int fd, struct packet *p);
void read_total_raw_bytes_length(int fd, struct packet *p);
void read_last_bytes(int fd, struct packet *p);

unsigned char read_byte(int fd, struct packet *p);
unsigned int read_int(int fd, struct packet *p);
char *read_string(int fd, struct packet *p, int length);

/* ========================================================= */
/*                            MAIN                           */
/* ========================================================= */

int main(int argc, char **argv)
{
    start_broker_server();
    return 0;
}

/* ========================================================= */
/*                   FUNCTION DEFINITIONS                    */
/* ========================================================= */

void start_broker_server()
{
    /* Os sockets. Um que será o socket que vai escutar pelas conexões
     * e o outro que vai ser o socket específico de cada conexão */
    int listenfd, connfd;
    /* Informações sobre o socket (endereço e porta) ficam nesta struct */
    struct sockaddr_in servaddr;
    /* Retorno da função fork para saber quem é o processo filho e
     * quem é o processo pai */
    pid_t childpid;

    /* Criação de um socket. É como se fosse um descritor de arquivo.
     * É possível fazer operações como read, write e close. Neste caso o
     * socket criado é um socket IPv4 (por causa do AF_INET), que vai
     * usar TCP (por causa do SOCK_STREAM), já que o MQTT funciona sobre
     * TCP, e será usado para uma aplicação convencional sobre a Internet
     * (por causa do número 0) */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket :(\n");
        exit(2);
    }

    /* Agora é necessário informar os endereços associados a este
     * socket. É necessário informar o endereço / interface e a porta,
     * pois mais adiante o socket ficará esperando conexões nesta porta
     * e neste(s) endereços. Para isso é necessário preencher a struct
     * servaddr. É necessário colocar lá o tipo de socket (No nosso
     * caso AF_INET porque é IPv4), em qual endereço / interface serão
     * esperadas conexões (Neste caso em qualquer uma -- INADDR_ANY) e
     * qual a porta. Neste caso será a porta padrão BROKER_PORT
     */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(BROKER_PORT));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1)
    {
        perror("bind :(\n");
        exit(3);
    }

    /* Como este código é o código de um servidor, o socket será um
     * socket passivo. Para isto é necessário chamar a função listen
     * que define que este é um socket de servidor que ficará esperando
     * por conexões nos endereços definidos na função bind. */
    if (listen(listenfd, LISTENQ) == -1)
    {
        perror("listen :(\n");
        exit(4);
    }

    printf("[MQQT Broker is open. Waiting connections in port %s.]\n", BROKER_PORT);
    printf("[To stop the server, press CTRL+c.]\n");

    /* O servidor no final das contas é um loop infinito de espera por
     * conexões e processamento de cada uma individualmente */
    for (;;)
    {
        /* O socket inicial que foi criado é o socket que vai aguardar
         * pela conexão na porta especificada. Mas pode ser que existam
         * diversos clientes conectando no servidor. Por isso deve-se
         * utilizar a função accept. Esta função vai retirar uma conexão
         * da fila de conexões que foram aceitas no socket listenfd e
         * vai criar um socket específico para esta conexão. O descritor
         * deste novo socket é o retorno da função accept. */
        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
        {
            perror("accept :(\n");
            exit(5);
        }

        /* Agora o servidor precisa tratar este cliente de forma
         * separada. Para isto é criado um processo filho usando a
         * função fork. O processo vai ser uma cópia deste. Depois da
         * função fork, os dois processos (pai e filho) estarão no mesmo
         * ponto do código, mas cada um terá um PID diferente. Assim é
         * possível diferenciar o que cada processo terá que fazer. O
         * filho tem que processar a requisição do cliente. O pai tem
         * que voltar no loop para continuar aceitando novas conexões.
         * Se o retorno da função fork for zero, é porque está no
         * processo filho. */
        if ((childpid = fork()) == 0)
        {
            /**** PROCESSO FILHO ****/
            printf("[Uma conexão aberta]\n");
            /* Já que está no processo filho, não precisa mais do socket
             * listenfd. Só o processo pai precisa deste socket. */
            close(listenfd);

            /* Agora pode ler do socket e escrever no socket. Isto tem
             * que ser feito em sincronia com o cliente. Não faz sentido
             * ler sem ter o que ler. Ou seja, neste caso está sendo
             * considerado que o cliente vai enviar algo para o servidor.
             * O servidor vai processar o que tiver sido enviado e vai
             * enviar uma resposta para o cliente (Que precisará estar
             * esperando por esta resposta)
             */

            /* ========================================================= */
            /*                            EP1                            */
            process_mqtt(connfd);
            /* ========================================================= */

            /* Após ter feito toda a troca de informação com o cliente,
             * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            close(connfd);
    }
    exit(0);
}

void process_mqtt(int connfd)
{
    /* Armazena o packet atual */
    struct packet p;
    p.raw_bytes = (char *)malloc(MAXLINE * sizeof(char));
    /* Armazena os dados que serão enviados */
    struct vector send_data;

    /* CONNECT */
    /* TODO: Consider Flags */
    read_fixed_header(connfd, &p);
    if (p.type != CONNECT)
    {
        perror("connect :(\n");
        exit(6);
    }
    read_last_bytes(connfd, &p);

    /* CONNACK */
    /* TODO: Consider Flags and Different responses */
    send_data.length = 4;
    send_data.array = (char *)malloc(4 * sizeof(char));
    send_data.array[0] = 0x20;
    send_data.array[1] = 0x02;
    send_data.array[2] = 0;
    send_data.array[3] = 0;
    write(connfd, send_data.array, send_data.length);
    free(send_data.array);

    char *who = (char *)malloc(100 * sizeof(char));
    for (;;)
    {
        read_fixed_header(connfd, &p);
        if (p.type == UNEXISTING)
            break;

        if (p.type == PUBLISH)
        {
            sprintf(who, "(pub: %u%u)", (unsigned int)p.id_msb, (unsigned int)p.id_lsb);

            /* TODO: Consider Flags */
            unsigned int topic_length = read_int(connfd, &p);
            char *topic = read_string(connfd, &p, topic_length);
            unsigned long int message_length = p.total_raw_bytes_length - p.raw_bytes_length;
            char *message = read_string(connfd, &p, message_length);
            read_last_bytes(connfd, &p);

            /* Send message to fifo */
            send_message(topic, p.raw_bytes, p.raw_bytes_length);
            printf("%s > Sent message to topic '%s': '%s'\n", who, topic, message);
            free(topic);
            free(message);
        }
        else if (p.type == SUBSCRIBE)
        {
            sprintf(who, "(sub: %u%u)", (unsigned int)p.id_msb, (unsigned int)p.id_lsb);

            /* TODO: Consider Flags */
            p.id_msb = read_byte(connfd, &p);
            p.id_lsb = read_byte(connfd, &p);
            unsigned int topic_length = read_int(connfd, &p);
            char *topic = read_string(connfd, &p, topic_length);
            read_last_bytes(connfd, &p);

            /* SUBACK */
            send_data.length = 5;
            send_data.array = (char *)malloc(4 * sizeof(char));
            send_data.array[0] = 0x90;
            send_data.array[1] = 0x03;
            send_data.array[2] = p.id_msb;
            send_data.array[3] = p.id_lsb;
            send_data.array[4] = 0;
            write(connfd, send_data.array, send_data.length);
            free(send_data.array);

            printf("%s > Subscribed to topic '%s'\n", who, topic);

            if (fork() == 0)
            {
                /* TRANSMIT PUBLISH PACKETS */
                char *fifo_path = create_fifo(topic);
                for (;;)
                {
                    int fifo_fd = open(fifo_path, O_RDONLY);
                    read_fixed_header(fifo_fd, &p);
                    read_last_bytes(fifo_fd, &p);

                    /* PUBLISH (sent from Server to a Client) */
                    write(connfd, p.raw_bytes, p.raw_bytes_length);
                    printf("%s > Received message from topic '%s' and sent to client\n", who, topic);

                    close(fifo_fd);
                }
                free(fifo_path);
                exit(0);
            }
            /* else: HANDLE MORE CONTROL PACKETS */
            free(topic);
        }
        else if (p.type == PINGREQ)
        {
            read_last_bytes(connfd, &p);

            /* PINGRESP */
            send_data.length = 2;
            send_data.array = (char *)malloc(2 * sizeof(char));
            send_data.array[0] = 0xD0;
            send_data.array[1] = 0x0;
            write(connfd, send_data.array, send_data.length);
            free(send_data.array);

            if (SHOW_LOG)
                printf("%s > Received and answered a ping\n", who);
        }
        else
            printf("%s > Received unknown packet, type: %d\n", who, p.type);

        if (SHOW_LOG)
            printf("%s > Waiting new packet\n", who);
    }

    printf("%s > No more messages\n", who);
    free(p.raw_bytes);
}

/* ========================================================= */
/*                          PARSING                          */
/* ========================================================= */

void read_fixed_header(int fd, struct packet *p)
{
    int length = read(fd, p->raw_bytes, 1);
    if (length <= 0)
    {
        p->type = UNEXISTING;
        return;
    }
    p->raw_bytes_length = 1;

    p->flags = p->raw_bytes[0] & 0x0F;       /* 4 low bits of byte */
    p->type = (p->raw_bytes[0] >> 4) & 0x0F; /* 4 high bits of byte */

    read_total_raw_bytes_length(fd, p);
}

void read_total_raw_bytes_length(int fd, struct packet *p)
{
    int multiplier = 1;
    p->total_raw_bytes_length = 0;
    char encoded_byte = -1;
    do
    {
        int length = read(fd, &encoded_byte, 1);
        p->raw_bytes[p->raw_bytes_length] = encoded_byte;
        p->raw_bytes_length++;

        p->total_raw_bytes_length += (encoded_byte & 127) * multiplier;
        multiplier *= 128;
    } while ((encoded_byte & 128) != 0);
    p->total_raw_bytes_length += p->raw_bytes_length;
}

void read_last_bytes(int fd, struct packet *p)
{
    long length = read(fd, &p->raw_bytes[p->raw_bytes_length], p->total_raw_bytes_length - p->raw_bytes_length);
    p->raw_bytes_length += length;
}

/* ========================================================= */
/*                       USUAL READS                         */
/* ========================================================= */

unsigned char read_byte(int fd, struct packet *p)
{
    read(fd, &p->raw_bytes[p->raw_bytes_length], 1);
    char c = p->raw_bytes[p->raw_bytes_length];
    p->raw_bytes_length += 1;
    return c;
}

unsigned int read_int(int fd, struct packet *p)
{
    read(fd, &p->raw_bytes[p->raw_bytes_length], 2);

    char msb_id = p->raw_bytes[p->raw_bytes_length];
    char lsb_id = p->raw_bytes[p->raw_bytes_length + 1];
    unsigned int integer = construct_int(lsb_id, msb_id);

    p->raw_bytes_length += 2;

    return integer;
}

char *read_string(int fd, struct packet *p, int length)
{
    read(fd, &p->raw_bytes[p->raw_bytes_length], length);
    char *str = copy_str(&p->raw_bytes[p->raw_bytes_length], length);
    p->raw_bytes_length += length;
    return str;
}
