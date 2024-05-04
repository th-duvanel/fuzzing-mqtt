/* Exercício Programa 1 - MAC0352 - Redes de Computadores e Sistemas Distribuídos
 * Broker MQTT
 * 
 * Adaptação de um servidor echo disponibilizado pela disciplina
 * Adpatdo por: Daniel Silva Lopes da Costa - NUPS 11302720
 * Em 24/04/2022
 * 
 * Para mais detalhes consulte os documentos na pasta docs.
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include "definitions.h"
#include "utils.h"

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
    char recvline[MAXLINE + 1];
    /* Armazena o tamanho da string lida do cliente */
    ssize_t n;

    /* Alocando memória para a lista de PIDs, onde cada posição pode representar um subscriber*/
    int *pid_list = malloc_global(MAX_NUMBER_PROCESS * sizeof(int));
    for (int j = 0; j < MAX_NUMBER_PROCESS; j++)
    {
        pid_list[j] = -1;
    }

    /* Memória alocada para salvar os tópicos ativos no broker */
    char **topics_list = malloc_global(MAX_NUMBER_TOPICS * sizeof(char *));
    for (int i = 0; i < MAX_NUMBER_TOPICS; i++)
    {
        topics_list[i] = malloc_global(MAX_TOPIC_LEN * sizeof(char));
    }

    /*Indentificador para o tópico*/
    int id_topic = -1;

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <Porta>\n", argv[0]);
        exit(1);
    }

    /* Criação de um socket. */
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
    printf("[O número do processo é %d]\n", getpid());

    /* O servidor no final das contas é um loop infinito de espera por
     * conexões e processamento de cada uma individualmente */

    for (;;)
    {

        if ((connfd = accept(listenfd, (struct sockaddr *)NULL, NULL)) == -1)
        {
            perror("accept :(\n");
            exit(5);
        }

        /* Agora o servidor precisa tratar este cliente de forma
         * separada. Para isto é criado um processo filho usando a
         * função fork.
         *  */
        if ((childpid = fork()) == 0)
        {
            /**** PROCESSO FILHO ****/
            printf("[Uma conexão aberta no processo %d]\n", getpid());
            /* Já que está no processo filho, não precisa mais do socket
             * listenfd. Só o processo pai precisa deste socket. */
            close(listenfd);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */
            while ((n = read(connfd, recvline, MAXLINE)) > 0)
            {
                recvline[n] = 0;
                int count = 0;
                int packetType = abs((int)recvline[count] >> 4);
                u_int64_t remaning_length = recvline[count + 1];

                switch (packetType)
                {
                case CONNECT:
                    printf("\nCONNECT packet\n");

                    // Create and send CONNAK Packet
                    int connack_size = 2;
                    u_int8_t *connack_packet;
                    connack_packet = malloc(4 * sizeof(u_int8_t));
                    connack_packet[0] = CONNACK << 4;
                    connack_packet[1] = connack_size;
                    connack_packet[2] = 0x00;
                    connack_packet[3] = 0x00;
                    write(connfd, connack_packet, 4);
                    free(connack_packet);

                    break;
                case PUBLISH:
                    printf("\nPUBLISH packet\n");

                    // Topic
                    count = 2;
                    u_int16_t topic_size = (1 << 8) * recvline[count] + recvline[count + 1];
                    count += 2;
                    char *topic_value_publish = malloc((topic_size + 1) * sizeof(char));
                    memcpy(topic_value_publish, &recvline[count], topic_size);
                    topic_value_publish[topic_size] = 0;
                    printf("Topic size: %d\n", topic_size);
                    printf("Topic value publisher: %s\n", topic_value_publish);

                    // Message
                    count += topic_size;
                    int msg_size = remaning_length - (count - 2);
                    unsigned char *msg_pub = malloc((msg_size + 1) * sizeof(unsigned char));
                    memcpy(msg_pub, &recvline[count], msg_size);
                    msg_pub[msg_size] = 0;
                    printf("Message size: %d\n", msg_size);
                    printf("Message publisher: %s\n", msg_pub);

                    //Recvline
                    int recvline_len = 2 + remaning_length;
                    unsigned char *recvline_pub = malloc(recvline_len * sizeof(unsigned char));
                    memcpy(recvline_pub, recvline, recvline_len);
                    printf("Tamanho da mensagem é :%d\n", recvline_len);

                    // Write the message in the pipe of it subscriber to a topic
                    id_topic = findTopic(topics_list, topic_value_publish);
                    if (id_topic == -1)
                    {
                        printf("Tópico inválido - Limite de tópicos atingido!\n");
                        break;
                    }
                    else
                    {
                        for (int j = 0; j < MAX_NUMBER_PROCESS; j++)
                        {
                            if (pid_list[j] == id_topic)
                            {
                                char *filename = malloc(FILENAME_LEN * sizeof(char));
                                char pid_name[8];
                                int pid;
                                int file_pd;
                                pid = j;

                                /* Constroi o nome do arquivo */
                                sprintf(pid_name, "%d", pid);
                                strcat(strcpy(filename, "/tmp/temp.mac0352."), pid_name);
                                printf("Publisher do tópico: %s vai escrever no arquivo: %s a mensagem %s\n", topic_value_publish, filename, msg_pub);

                                /*Escreve a nova mensagem no arquivo*/
                                file_pd = open(filename, O_WRONLY);
                                unlink((const char *)filename);
                                write(file_pd, recvline_pub, recvline_len);
                                close(file_pd);
                                free(filename);
                            }
                        }
                    }
                    // Free memory
                    free(recvline_pub);
                    free(topic_value_publish);
                    free(msg_pub);
                    break;
                case SUBSCRIBE:
                    printf("\nSUBSCRIBE packet\n");
                    // Length of the topic
                    count = 4;
                    u_int16_t topic_len = ((1 << 8) * (int)recvline[count]) + (int)recvline[count + 1];
                    count += 2;

                    // Value of the topic
                    char *topic_value_subscribe = malloc_global((topic_len + 1) * sizeof(char));
                    memcpy(topic_value_subscribe, &recvline[count], topic_len);
                    topic_value_subscribe[topic_len] = 0;
                    printf("Topic value subscriber: %s\n", topic_value_subscribe);

                    // SUBACK
                    int suback_size = 6;
                    u_int8_t *suback_package = malloc(6 * sizeof(u_int8_t));
                    suback_package[0] = 0x90;
                    suback_package[1] = 0x04;
                    suback_package[2] = recvline[2]; //0x00;
                    suback_package[3] = recvline[3]; //0x01;
                    suback_package[4] = 0x00;

                    id_topic = findTopic(topics_list, topic_value_subscribe);
                    // New topic
                    if (id_topic == -1)
                        id_topic = findSpot(topics_list);
                    // No space for new topic
                    if (id_topic == -1)
                    {
                        printf("Número máximo de tópicos atingido!!\n");
                        // Send SUBACK - Failure Case
                        suback_package[suback_size - 1] = 0x80;
                        write(connfd, suback_package, 6);
                        free(suback_package);
                    }
                    else
                    {
                        // Send SUBACK - Success Case
                        suback_package[suback_size - 1] = 0x00;
                        write(connfd, suback_package, 6);
                        free(suback_package);

                        //Save the topic value
                        strcpy(topics_list[id_topic], topic_value_subscribe);

                        // Create PIPE to  subscriber - Filename = /tmp/temp.mac0352.xxxxxx pid = xxxxxx
                        char *filename = malloc_global(FILENAME_LEN * sizeof(char));
                        char pid_name[8];
                        int pid = getpid();
                        pid_list[pid] = id_topic;
                        sprintf(pid_name, "%d", pid);
                        strcat(strcpy(filename, "/tmp/temp.mac0352."), pid_name);
                        if (mkfifo((const char *)filename, 0644) == -1)
                        {
                            perror("mkfifo :(\n");
                        }
                        printf("Escrito no tópico: %s - Lendo do pipe: %s \n", topic_value_subscribe, filename);

                        int file_pd = open(filename, O_RDONLY);
                        unsigned char *msg_pubTOsub = malloc((MAXLINE + 1) * sizeof(unsigned char));

                        // Infinity loop listen to the pipe
                        while (1)
                        {
                            if ((n = read(file_pd, msg_pubTOsub, MAXLINE)) > 0)
                            {
                                msg_pubTOsub[n] = 0;
                                write(connfd, msg_pubTOsub, n);
                            }
                        }
                        close(file_pd);
                        free_global(filename, FILENAME_LEN * sizeof(char));
                        free(msg_pubTOsub);
                        pid_list[pid] = -1;
                    }
                    break;
                case PINGREQ:
                    printf("\nPINGREQ packet\n");
                    /* PINGRESP */
                    u_int8_t pingersp_packet[2];
                    pingersp_packet[0] = 0xd0;
                    pingersp_packet[1] = 0x00;
                    write(connfd, pingersp_packet, 2);
                    fprintf(stdout, "Sending pingresp...\n");
                    break;
                default:
                    close(connfd);
                    printf("Reserved value\n");
                    break;
                }
            }
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */
            /* Finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
            /**** PROCESSO PAI ****/
            close(connfd);
    }

    /* Liberar o espaço de memória reservado */
    for (int i = 0; i < MAX_NUMBER_TOPICS; i++)
    {
        free_global(topics_list[i], MAX_TOPIC_LEN * sizeof(char));
    }
    free_global(topics_list, MAX_NUMBER_TOPICS * sizeof(char *));
    free_global(pid_list, MAX_NUMBER_PROCESS * sizeof(int));

    exit(0);
}