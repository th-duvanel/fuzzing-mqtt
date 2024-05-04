/* EP1 - REDES DE COMPUTADORES E SISTEMAS DISTRIBUÍDOS (MAC0352)
 * NOME: WILLIAN HIROSHI TAKIHI
 * NUSP: 11221755
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
#include <math.h>
#include <sys/wait.h>
#include <sys/types.h>

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>

#define ARRAYSIZE(x) (sizeof(x) / sizeof((x)[0]))
#define MAXTOPICNAME 120

#define LISTENQ 1
#define MAXLINE 4096

/* Coloca a representação de um char em um array de bits */
void charToBit(char character, int bit[])
{
    for (int i = 0; i < 8; i++)
        bit[i] = !!((character << i) & 0x80);
}

/* Converte array de bits em decimal */
int convertToDecimal(int bit[], int arraySize)
{
    int dec = 0;
    int expo = arraySize - 1;

    for (int i = 0; i < arraySize; i++)
        dec += bit[i] * pow(2, expo - i);

    return dec;
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
    char recvline[MAXLINE + 1];
    /* Armazena o tamanho da string lida do cliente */
    ssize_t n;

    /* File Descriptor para escrita no arquivo do tópico */
    int fd;
    char fileName[MAXLINE];

    if (argc != 2)
    {
        fprintf(stderr, "Uso: %s <Porta>\n", argv[0]);
        fprintf(stderr, "Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

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
     * qual a porta. Neste caso será a porta que foi passada como
     * argumento no shell (atoi(argv[1]))
     */
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(atoi(argv[1]));
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

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n", argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

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
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */
            while ((n = read(connfd, recvline, MAXLINE)) > 0)
            {
                int bit[8];

                charToBit(recvline[0], bit);
                int controlPacketTypeBytes[4] = {bit[0], bit[1], bit[2], bit[3]};
                int controlPacketType = convertToDecimal(controlPacketTypeBytes, ARRAYSIZE(controlPacketTypeBytes));

                switch (controlPacketType)
                {
                    int nBytes,
                        currentByte,
                        remainingLength,
                        nBytesTopicName,
                        nBytesPayload,
                        packetIdentifierMSB,
                        packetIdentifierLSB;

                    char topicName[MAXTOPICNAME],
                        payload[MAXLINE],
                        sendPacket[MAXLINE];

                /* Cliente está requisitando conexão ao servidor (CONNECT) */
                /* CONNACK em resposta */
                case 1:;
                    nBytes = 4;
                    memset(sendPacket, '\0', sizeof(sendPacket));

                    sendPacket[0] = 32; // Control Packet Type
                    sendPacket[1] = 2;  // Remaining length
                    sendPacket[2] = 0;  // Variable header, 0 = CleanSession == 1
                    sendPacket[3] = 0;  // Connect Return Code, 0 = Connection Accepted
                    write(connfd, sendPacket, nBytes);

                    break;

                /* PUBLISH mensagem em um tópico */
                case 3:;
                    /* Leitura do pacote recebido */
                    currentByte = 1;

                    charToBit(recvline[currentByte], bit);
                    remainingLength = convertToDecimal(bit, ARRAYSIZE(bit));

                    currentByte = 3;
                    charToBit(recvline[currentByte], bit);
                    nBytesTopicName = convertToDecimal(bit, ARRAYSIZE(bit));
                    nBytesPayload = remainingLength - nBytesTopicName - 2;

                    currentByte = 4;
                    memset(topicName, '\0', sizeof(topicName));
                    memset(payload, '\0', sizeof(payload));

                    while (nBytesTopicName--)
                    {
                        topicName[strlen(topicName)] = recvline[currentByte];
                        currentByte++;
                    }

                    while (nBytesPayload--)
                    {
                        payload[strlen(payload)] = recvline[currentByte];
                        currentByte++;
                    }

                    /* Remove pacote de DISCONNECT que vem às vezes no final do pacote */
                    nBytes = n;
                    currentByte = 1;
                    charToBit(recvline[currentByte], bit);
                    remainingLength = convertToDecimal(bit, ARRAYSIZE(bit));
                    if (nBytes > remainingLength + 2)
                        nBytes = nBytes - 2;

                    /* Escrita da mensagem no tópico */
                    printf("[Um cliente publicou a mensagem: <%s> no tópico: <%s>]\n", payload, topicName);
                    memset(fileName, 0, sizeof(fileName));
                    strcpy(fileName, "/tmp/");
                    strcat(fileName, topicName);

                    // Abre o file descriptor do tópico para escrita
                    // Escreve sempre no final do arquivo e o cria caso não exista
                    fd = open((char *)fileName, O_WRONLY | O_APPEND | O_CREAT, 0644);
                    if (fd == -1)
                    {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // LOCK a leitura do arquivo, quando terminado UNLOCK
                    lockf(fd, F_LOCK, 0);
                    write(fd, recvline, nBytes);
                    usleep(100000);
                    lockf(fd, F_ULOCK, 0);

                    break;

                /* Cliente requisita um SUBSCRIBE */
                case 8:;
                    /* Leitura do pacote recebido */
                    currentByte = 1;

                    charToBit(recvline[currentByte], bit);
                    remainingLength = convertToDecimal(bit, ARRAYSIZE(bit));

                    currentByte = 2;
                    charToBit(recvline[currentByte], bit);
                    packetIdentifierMSB = convertToDecimal(bit, ARRAYSIZE(bit));

                    currentByte = 3;
                    charToBit(recvline[currentByte], bit);
                    packetIdentifierLSB = convertToDecimal(bit, ARRAYSIZE(bit));

                    currentByte = 5;
                    charToBit(recvline[currentByte], bit);
                    nBytesTopicName = convertToDecimal(bit, ARRAYSIZE(bit));

                    currentByte++;
                    memset(topicName, '\0', sizeof(topicName));

                    while (nBytesTopicName--)
                    {
                        topicName[strlen(topicName)] = recvline[currentByte];
                        currentByte++;
                    }

                    /* Mensagem SUBACK em resposta à requisição de SUBSCRIBE feita pelo cliente */
                    nBytes = 5;
                    memset(sendPacket, '\0', sizeof(sendPacket));

                    sendPacket[0] = 144;                 // Control Packet Type
                    sendPacket[1] = 3;                   // Remaining length
                    sendPacket[2] = packetIdentifierMSB; // Packet Identifier MSB (Mesmo recebido do packet SUBSCRIBE)
                    sendPacket[3] = packetIdentifierLSB; // Packet Identifier LSB (Mesmo recebido do packet SUBSCRIBE)
                    sendPacket[4] = 0;                   // Return Code Success
                    write(connfd, sendPacket, nBytes);   // Send Packet
                    printf("[Um cliente se inscreveu no tópico: <%s>]\n", topicName);

                    /* Leitura da mensagem no tópico */
                    memset(fileName, 0, sizeof(fileName));
                    strcpy(fileName, "/tmp/");
                    strcat(fileName, topicName);

                    fd = open((char *)fileName, O_RDONLY | O_CREAT, 0644);
                    if (fd == -1)
                    {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // Coloca o ponteiro para leitura no final do arquivo
                    if (lseek(fd, 0, SEEK_END) == -1)
                    {
                        perror("lseek");
                        exit(EXIT_FAILURE);
                    }

                    // Verifica se uma mensagem foi escrita no tópico
                    while (1)
                    {
                        memset(recvline, '\0', sizeof(recvline));
                        nBytes = read(fd, recvline, MAXLINE);

                        // Envia Pacote para o cliente
                        if (nBytes > 0)
                            write(connfd, recvline, nBytes);

                        usleep(10000);
                    }

                    break;

                /* Requisição de PING pelo cliente SUBSCRIBE */
                case 12:
                    /* Manda um pacote de resposta */
                    nBytes = 2;
                    memset(sendPacket, '\0', sizeof(sendPacket));

                    sendPacket[0] = 208;               // Control Packet Type
                    sendPacket[1] = 0;                 // Remaining length
                    write(connfd, sendPacket, nBytes); // Send Packet

                    break;

                /* Cliente está requisitando um DISCONNECT */
                case 14:
                    close(connfd);
                    break;
                }
            }

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */

            /* Após ter feito toda a troca de informação com o cliente,
             * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
        {

            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            signal(SIGCHLD, SIG_IGN);
            close(connfd);
        }
    }
    exit(0);
}
