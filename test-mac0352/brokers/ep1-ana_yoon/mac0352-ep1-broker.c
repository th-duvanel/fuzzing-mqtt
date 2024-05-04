/*
 *  EP1 - MAC0352
 *  Nome: Ana Yoon Faria de Lima
 *  NUSP: 11795273
 * 
 *  Código base utilizado:
 *  mac-0352-servidor-exemplo-ep1.c
 *  (Por Prof. Daniel Batista <batista@ime.usp.br>)
 *  
 *  
 *  Bibliografia:
 * 
 *  http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html
 *  https://stackoverflow.com/questions/10818658/parallel-execution-in-c-program
 *  https://man7.org/linux/man-pages/man7/pthreads.7.html
 *  https://www.geeksforgeeks.org/multithreading-c-2/
 *  https://linux.die.net/man/3/open
 *  https://www.decodeschool.com/C-Programming/File-Operations/C-Program-to-read-all-the-files-located-in-the-specific-directory
 * 
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
#include <stdbool.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>
#include <pthread.h>
#include <fcntl.h>
#include <dirent.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

bool isBetween(int number, int lowerBound, int upperBound) {
    return number >= lowerBound && number <= upperBound;
}

bool StartsWith(const char *stringToVerify, const char *expectedStart)
{

    if(strncmp(stringToVerify, expectedStart, strlen(expectedStart)) == 0) {
        return 1;
    }
    return 0;
}

int getNumberOfDigits(int number) {
    return (int) log10(number) + 1;
}

int returnCodeRespondeConnect(char* recvline) {
    // verificar se conectou de fato, para mandar a resposta.
    // se a conexão falhou, deve retornar um código não nulo.

    // verificar se o nome do protocolo é MQTT. Se não for, conexão é falsa
    if (recvline[2] != 0x00 || recvline[3] != 0x04){
        printf("Incorrect protocol name length.\n");
        return 1;
    }
    if (recvline[4] != 'M' || recvline[5] != 'Q' || recvline[6] != 'T' || recvline[7] != 'T'){
        printf("Nome do protocolo incorreto.\n");
        return 1;
    }
    return 0;
}

struct infoPacketSubscribe{
    int* connfd;
    char* filePath;
};

void* ThreadReadSocket(void *infoPacketSubscribeArguments) {
    int n;
    struct infoPacketSubscribe* infoPacket = infoPacketSubscribeArguments;
    int connfd = *infoPacket->connfd;
    char* filePath = infoPacket->filePath;
    unsigned char recvline[MAXLINE + 1];
    while ((n = read(connfd, recvline, MAXLINE)) > 0) {
        int firstDigit = (int) recvline[0];
        if (firstDigit == 192) { // PINGREQ - 11000000
            // printf("PACOTE PINGREQ DENTRO DO SUB\n");

            /*
                Tratamento feito de forma análoga ao PINGREQ no while principal
            */

            int remainingLength = (int) recvline[1];

            if (remainingLength != 0) {
                printf("Erro: PINGREQ com remainingLength diferente de zero");
                exit(1);
            }

            char pingresHeader[2] = {208, 0};
            int lengthPingresHeader = sizeof pingresHeader/ sizeof pingresHeader[0];
            write(connfd, pingresHeader, lengthPingresHeader); 

        }
        else if (firstDigit == 224) { // DISCONNECT - 1110
            // printf("PACOTE DISCONNECT DENTRO DO SUB\n");
            break; // disconectar, sair do loop de ficar lendo.
        }
    }
    int fileDescriptor = open(filePath, O_WRONLY | O_NONBLOCK);
    close(fileDescriptor);
    unlink((const char*) filePath);
    return NULL;
}

void* ThreadReadFile(void *infoPacketSubscribeArguments) {
    int n, i;
    unsigned char recvline[MAXLINE + 1];
    struct infoPacketSubscribe* infoPacket = infoPacketSubscribeArguments;
    int connfd = *infoPacket->connfd;
    char* filePath = infoPacket->filePath;
    // printf("FILE PATH THREAD: %s", filePath);

    int fileDescriptor = open(filePath, O_RDONLY);
    while ((n = read(fileDescriptor, recvline, MAXLINE)) > 0) {

        int remainingLength = (int) recvline[1];

        if (write(connfd, recvline, remainingLength + 2) < 0){
            printf("Erro no write do sub");
        }
        close(fileDescriptor);
        fileDescriptor = open(filePath, O_RDONLY);
    }
    close(fileDescriptor);
    return NULL;

    
}

int main (int argc, char **argv) {
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
   
    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    /* Criação de um socket. É como se fosse um descritor de arquivo.
     * É possível fazer operações como read, write e close. Neste caso o
     * socket criado é um socket IPv4 (por causa do AF_INET), que vai
     * usar TCP (por causa do SOCK_STREAM), já que o MQTT funciona sobre
     * TCP, e será usado para uma aplicação convencional sobre a Internet
     * (por causa do número 0) */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
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
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    /* Como este código é o código de um servidor, o socket será um
     * socket passivo. Para isto é necessário chamar a função listen
     * que define que este é um socket de servidor que ficará esperando
     * por conexões nos endereços definidos na função bind. */
    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   
    /* O servidor no final das contas é um loop infinito de espera por
     * conexões e processamento de cada uma individualmente */
	for (;;) {
        /* O socket inicial que foi criado é o socket que vai aguardar
         * pela conexão na porta especificada. Mas pode ser que existam
         * diversos clientes conectando no servidor. Por isso deve-se
         * utilizar a função accept. Esta função vai retirar uma conexão
         * da fila de conexões que foram aceitas no socket listenfd e
         * vai criar um socket específico para esta conexão. O descritor
         * deste novo socket é o retorno da função accept. */
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
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
        if ( (childpid = fork()) == 0) {
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
            /* TODO: É esta parte do código que terá que ser modificada
             * para que este servidor consiga interpretar comandos MQTT  */
             
            while ((n=read(connfd, recvline, MAXLINE)) > 0) {
                recvline[n]=0;
                printf("[Cliente conectado no processo filho %d enviou:] ",getpid());

                int firstDigit = (int) recvline[0];
                // printf("PRIMEIRO DIGITO: %d\n", firstDigit);
                if (firstDigit == 16) { // CONNECT - 00010000
                    printf("PACOTE CONNECT\n");

                    /*
                        Em resposta ao CONNECT, o servidor deve mandar um pacote CONNACK.
                        CONNACK PACKET FIXED HEADER
                        byte 1 - 00100000 = 2^5
                        byte 2 - 00000010 = 2^1
                        CONNACK PACKET VARIABLE HEADER
                        byte 1 - 0000000X - Connect Acknowledge Flags
                        byte 2 - XXXXXXXX - Connect Return code
                    
                    */
                   
                    int connectReturnCode = returnCodeRespondeConnect(recvline);
                    char connackHeader[4] = {32, 2, 1, connectReturnCode};
                    int lengthConnackHeader = sizeof connackHeader/ sizeof connackHeader[0];
                    write(connfd,connackHeader, lengthConnackHeader);
                }
                else if (isBetween(firstDigit, 48, 63)) { // PUBLISH - 0011XXXX
                    printf("PACOTE PUBLISH\n");

                    /*
                        PUBLISH PACKET FIXED HEADER
                        São os dois primeiros bytes.
                        byte 1 - DUP, QoS, RETAIN
                        byte 2 - Remaining Length field - tamanho do variable header mais o tamanho do payload.
                                 Vamos usar essa informação para saber o tamanho da mensagem, que vem no payload
                        PUBLISH PACKET VARIABLE HEADER
                        byte 1 - length MSB (Most significant bit) - a partir de 2^8 - multiplica por 256
                        byte 2 - length LSB (Less significant bit) - de 2^0 a 2^7
                        A partir do byte 3, cada byte irá conter uma letra do nome do tópico, até que todo nome esteja escrito.
                        PAYLOAD
                        contém a mensagem que está sendo publicada
                    */
                   

                    int topicLength = (int) recvline[2] * 256 + (int) recvline[3];
                    char* topicName = malloc((topicLength + 2) * sizeof(char));
                    int remainingLength = (int) recvline[1];
                    int messageSentLength = remainingLength - (2 + topicLength);
                    char* messageSent = malloc(messageSentLength * sizeof(char));
                    int i;
                    for (i = 0; i < topicLength; i++) {
                        topicName[i] = recvline[i + 4];
                    }
                    printf("TOPIC NAME = %s\n", topicName);
                    for (i = 0; i < messageSentLength; i++) {
                        messageSent[i] = recvline[topicLength + i + 4];
                    }
                    printf("MESSAGE SENT = %s\n", messageSent);
                    

                    /*
                        Agora, precisamos mandar a mensagem enviada para todos os clientes inscritos no tópico.
                        Para isso, nessa implementação, faremos um loop por todos os arquivos dos clientes
                        inscritos no tópico, escrevendo a mensagem enviada. De forma análoga ao que foi mostrado
                        no arquivo ep1+pipe, do enunciado, guardaremos esses arquivos na pasta /tmp
                        Para isso, usaremos a biblioteca dirent
                    */


                    
                    char *fileNameStartsWith = (char*)malloc((strlen("temp.mac0352.1.") + topicLength + 2) * sizeof(char));
                    sprintf(fileNameStartsWith, "temp.mac0352.ep1.%s.", topicName);

                    /*
                    printf("fileNameStartsWith:\n");
                    for (i = 0; i < strlen("temp.mac0352.1.") + topicLength + 2; i++) {
                        printf("%c",fileNameStartsWith[i]);
                    }
                    printf("\n");
                    */

                    // printf("fileNameStartsWith: %s\n", fileNameStartsWith);

                    DIR *directory;
                    struct dirent *dir;

                    directory = opendir("/tmp");
                    if (directory)
                    {
                        while ((dir = readdir(directory)) != NULL)
                        {
                            if (StartsWith(dir->d_name, fileNameStartsWith)) {
                                // printf("FILE NAME: %s\n", dir->d_name);

                                // arquivo representa cliente inscrito no tópico
                                // Portanto, vamos escrever a mensagem publicada nesse arquivo
                                int filePathLength = strlen("/tmp/") + strlen(dir->d_name) + 2;
                                char *filePath = (char*)malloc((filePathLength) * sizeof(char));
                                sprintf(filePath, "/tmp/%s", dir->d_name);
                                // printf("FILE PATH: %s\n", filePath);
                                int fileDescriptor = open(filePath, O_WRONLY | O_NONBLOCK);
                                write(fileDescriptor, recvline, remainingLength + 2);
                                close(fileDescriptor);
                            }
                        }
                    }

                    
                    // FREE DOS MALLOCS
                    free(topicName);
                    free(messageSent);

                }
                else if (firstDigit == 130) { // SUBSCRIBE - 10000010
                    printf("PACOTE SUBSCRIBE\n");

                    /*
                        SUBSCRIBE PACKET FIXED HEADER
                        São os dois primeiros bytes.
                        byte 1 - Identifica o SUBSCRIBE
                        byte 2 - Remaining Length field - tamanho do variable header (2 bytes) mais o tamanho do payload.
                                 Vamos usar essa informação para saber o tamanho da mensagem, que vem no payload
                        SUBSCRIBE PACKET VARIABLE HEADER
                        byte 1 - Packet Identifier MSB (Most significant bit) - a partir de 2^8 - multiplica por 256
                        byte 2 - Packet Identifier LSB (Less significant bit) - de 2^0 a 2^7
                        PAYLOAD
                        Vamos considerar somente um tópico por subscribe.
                        Assim, temos:
                        byte 1 - Length MSB
                        byte 2 - Length LSB
                        Além disso, o payload conterá o nome do tópico na qual o cliente deseja se inscrever.
                        A partir do byte 3, cada byte irá conter uma letra do nome do tópico, até que todo nome esteja escrito.
                    */

                    int topicLength = (int) recvline[4] * 256 + (int) recvline[5];
                    char* topicName = malloc((topicLength + 2) * sizeof(char));
                    int remainingLength = (int) recvline[1];
                    int i;
                    for (i = 0; i < topicLength; i++) {
                        topicName[i] = recvline[i + 6];
                    }
                    printf("TOPIC NAME = %s\n", topicName);

                    /*
                        Em resposta ao SUBSCRIBE, o servidor deve mandar um pacote SUBACK.
                        SUBACK PACKET FIXED HEADER
                        byte 1 - 10010000 = 2^7 + 2^4 = 144
                        byte 2 - Remaining Length
                        CONNECT PACKET VARIABLE HEADER
                        byte 1 - Packet Identifier MSB
                        byte 2 - Packet Identifier LSB
                        PAYLOAD
                        byte 1 - Return Code
                        Da documentação, temos:
                        "
                            Allowed return codes:
                            0x00 - Success - Maximum QoS 0 
                            0x01 - Success - Maximum QoS 1 
                            0x02 - Success - Maximum QoS 2 
                            0x80 - Failure
                        "
                    */
                   
                    int packetIdentifierMSB = (int) recvline[2];
                    int packetIdentifierLSB = (int) recvline[3];
                    int subscribeReturnCode = 0; // 0, 1 e 2 indicam sucesso. 80 indica falha. Vamos supor sucesso
                    char subackHeader[5] = {144, 3, packetIdentifierMSB, packetIdentifierLSB, subscribeReturnCode};
                    int lengthSubackHeader = 5;
                    write(connfd, subackHeader, lengthSubackHeader);

                    /*
                        Para cada cliente inscrito em determinado tópico, vamos criar um arquivo para esse cliente nesse tópico.
                        Quando algo for publicado nesse tópico, o publisher vai escrever nesse arquivo.
                        Assim, o cliente, para receber atualizações, precisará ficar lendo esse arquivo.
                        O nome do arquivo deve conter o nome do tópico, para que o pub possa identificá-lo para receber atualizações.
                        Além disso, deve ser único para cada cliente. Para isso, usaremos o pid.
                        Usaremos um padrão semelhante ao usado no arquivo ep1+pipe do enunciado:
                        temp.mac0352.ep1.TOPIC_NAME.PID
                    
                    */

                    int pid = getpid();

                    // printf("pid = %d\n", pid);

                    int pidLength = getNumberOfDigits(pid);

                    // printf("pidLength = %d\n", pidLength);

                    int filePathLength = strlen("/tmp/temp.mac0352.ep1.") + topicLength + pidLength + 2;

                    char *filePath = (char*)malloc((filePathLength) * sizeof(char));
                    
                    sprintf(filePath, "/tmp/temp.mac0352.ep1.%s.%d", topicName, pid);
                    
                    /*
                    printf("NOME DO ARQUIVO QUE VOU CRIAR NO SUB:\n");
                    for (i = 0; i < filePathLength; i++) {
                        printf("%c",filePath[i]);
                    }
                    printf("\n");
                    */


                    if (mkfifo((const char *) filePath, 0644) == -1) {
                        perror("mkfifo :(\n");
                    }

                    /*
                        Precisamos agora que o cliente que se inscreveu fique fazendo duas coisas:
                         1 - Fique lendo do seu arquivo para verificar se chegou algo do pub
                         2 - Fique lendo do socket para verificar se desconectou ou identificar erro no ping
                        
                        Para isso, faremos uso de threads (linhas de execução), para que o cliente consiga realizar
                        essas duas tarefas de forma "simultânea". Faremos uso da biblioteca pthread.

                    */

                    int lengthMSB = (int) recvline[4];
                    int lengthLSB = (int) recvline[5];

                    struct infoPacketSubscribe* infoPacketToThread = malloc(sizeof *infoPacketToThread);
                    infoPacketToThread->connfd = &connfd;
                    infoPacketToThread->filePath = filePath;
                    
                    pthread_t threadIds[2];

                    pthread_create(&(threadIds[0]), NULL, ThreadReadFile, infoPacketToThread);
                    pthread_create(&(threadIds[1]), NULL, ThreadReadSocket, infoPacketToThread);
                    for(i = 0; i < 2; i++) {
                        pthread_join(threadIds[i], NULL);
                    }

                    // FREE DOS MALLOCS
                    free(infoPacketToThread);
                    free(topicName);
                    free(filePath);

                }
                else if (firstDigit == 192) { // PINGREQ - 11000000
                    printf("PACOTE PINGREQ\n");

                    /*
                        PINGREQ PACKET FIXED HEADER
                        São os dois primeiros bytes.
                        byte 1 - Identifica o PINGREQ
                        byte 2 - Remaining Length - é 0 para o PINGREQ, pois ele não tem variable header nem payload
                                    Dessa forma, usaremos esse fato para identificar um erro. Se o byte 2 do pingreq não
                                    for igual a 0, identificaremos a presença de um erro.
                        
                    */

                    int remainingLength = (int) recvline[1];

                    if (remainingLength != 0) {
                        printf("Erro: PINGREQ com remainingLength diferente de zero");
                        exit(1);
                    }

                    /*
                        Em resposta ao PINGREQ, o servidor deve mandar um pacote PINGRES.
                        PINGRES PACKET FIXED HEADER
                        byte 1 - 11010000 = 2^7 + 2^6 + 2^4 = 208
                        byte 2 - Remaining Length (0)
                        PINGRES não tem variable header nem payload
                        "
                    */

                    char pingresHeader[2] = {208, 0};
                    int lengthPingresHeader = sizeof pingresHeader/ sizeof pingresHeader[0];
                    write(connfd, pingresHeader, lengthPingresHeader);
                    

                }
                else if (firstDigit == 224) { // DISCONNECT - 1110
                    printf("PACOTE DISCONNECT\n");
                    break; // disconectar, sair do loop de ficar lendo.
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
            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            close(connfd);
    }
    exit(0);
}
