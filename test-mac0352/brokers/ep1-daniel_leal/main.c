/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 16/3/2022
 * 
 * Um código simples de um servidor de eco a ser usado como base para
 * o EP1. Ele recebe uma linha de um cliente e devolve a mesma linha.
 * Teste ele assim depois de compilar:
 * 
 * ./ep1-servidor-exemplo 8000
 * 
 * Com este comando o servidor ficará escutando por conexões na porta
 * 8000 TCP (Se você quiser fazer o servidor escutar em uma porta
 * menor que 1024 você precisará ser root ou ter as permissões
 * necessárias para rodar o código com 'sudo').
 *
 * Depois conecte no servidor via telnet. Rode em outro terminal:
 * 
 * telnet 127.0.0.1 8000
 * 
 * Escreva sequências de caracteres seguidas de ENTER. Você verá que o
 * telnet exibe a mesma linha em seguida. Esta repetição da linha é
 * enviada pelo servidor. O servidor também exibe no terminal onde ele
 * estiver rodando as linhas enviadas pelos clientes.
 * 
 * Obs.: Você pode conectar no servidor remotamente também. Basta
 * saber o endereço IP remoto da máquina onde o servidor está rodando
 * e não pode haver nenhum firewall no meio do caminho bloqueando
 * conexões na porta escolhida.
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
#include <ftw.h>
#include <dirent.h>
#include <math.h>
#include <signal.h>

#define LISTENQ 3
#define MAXDATASIZE 100
#define MAXLINE 65535

#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define SUBSCRIBE 8
#define SUBACK 9
#define PINGREQ 12
#define PINGRESP 13
#define DISCONNECT 14

#define ROOT_PATH "/tmp/redes"

struct packet {
    int packetType;
    char * body;
    int size;
};

typedef struct PacketLenght {
    int remainingLength;
    int numberOfBytes;
} PacketLength;

PacketLength decodeRemainingLength(char clientInput[MAXLINE + 1]) {
    int value = 0;
    int multiplier = 1;
    int index = 0;
    char byte;
    PacketLength packetLength;

    do {
        byte = clientInput[1 + index];
        value += (byte & 127) * multiplier;
        multiplier *= 128;
        index += 1;
        if (multiplier > 128 * 128 * 128) {
            printf("RemainingLenght :(");
            exit(1);
        }        
    } while ((byte & 128) != 0);

    packetLength.numberOfBytes = index;
    packetLength.remainingLength = value;
    return packetLength;
}


void printInput(char clientInput[MAXLINE + 1], int size) {
    for (int i = 0; i < size; i++) {
        printf("%02x ", clientInput[i]);
    }
    printf("\n");
}

int isSubscriber(struct packet input) {
    char fixedHeader = input.body[0];
    int packetType = (fixedHeader & 0xf0) >> 4;

    if (packetType == SUBSCRIBE) {
        return 1;
    }

    return 0;
}

int getMessageSize(char clientInput[MAXLINE + 1]) {
    PacketLength packetLength = decodeRemainingLength(clientInput);
    return packetLength.remainingLength + packetLength.numberOfBytes + 1;
}

char * getSubscriberTopicName(struct packet input) {
    PacketLength packetLength = decodeRemainingLength(input.body);
    int topicNameSizeFirstIndex = 1 + packetLength.numberOfBytes + 2;
    int topicNameSize = (input.body[topicNameSizeFirstIndex] * 256) + input.body[topicNameSizeFirstIndex + 1];
    char * topicName = malloc((topicNameSize) * sizeof(char));

    for (int i = 0; i < topicNameSize; i++) {
        topicName[i] = input.body[topicNameSizeFirstIndex + 2 + i];    
    }

    topicName[topicNameSize] = 0;
    return topicName;
}

char * getProducerTopicName(struct packet input) {
    PacketLength packetLength = decodeRemainingLength(input.body);
    int topicNameSizeFirstIndex = 1 + packetLength.numberOfBytes;
    int topicNameSize = (input.body[topicNameSizeFirstIndex] * 256) + input.body[topicNameSizeFirstIndex + 1];
    char * topicName = malloc(topicNameSize * sizeof(char));

    for (int i = 0; i < topicNameSize; i++)
        topicName[i] = input.body[topicNameSizeFirstIndex + 2 + i];    

    topicName[topicNameSize] = 0;
    return topicName;
}



void createRootDirectory() {
    if (access(ROOT_PATH, F_OK) == -1)
        if (mkdir(ROOT_PATH, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
            perror("Erro ao criar pasta raiz");
            exit(1);
    }
}

char * createTopicFile(char * topicName) {
    char * completeTopicDirectory = malloc(sizeof(char) * (strlen(topicName) + strlen(ROOT_PATH)));
    sprintf(completeTopicDirectory, "%s/%s", ROOT_PATH, topicName);

    if (access(completeTopicDirectory, F_OK) == -1)
        if (mkdir(completeTopicDirectory, S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
            perror("Erro ao criar pasta do tópico");
            exit(1);
    }
    
    pid_t pid = getpid();
    int numberOfDigits = log10(pid) + 1;
    char * completeFileName = malloc(sizeof(char) * (strlen(completeTopicDirectory) + numberOfDigits));
    sprintf(completeFileName, "%s/%d", completeTopicDirectory, pid);
    if (access(completeFileName, F_OK) == -1)
        if (mkfifo(completeFileName, 0644) == -1) {
            perror("mkfifo :(\n");
        }
    
    free(completeTopicDirectory);
    return completeFileName;
}

// Código achado no stackoverflow 
// https://stackoverflow.com/questions/5467725/how-to-delete-a-directory-and-its-contents-in-posix-c
int removeTopicPipeFile(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf){
    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

// Código achado no stackoverflow 
// https://stackoverflow.com/questions/5467725/how-to-delete-a-directory-and-its-contents-in-posix-c
int eraseTopicFilesAndFolders() {
    return nftw(ROOT_PATH, removeTopicPipeFile, 64, FTW_DEPTH | FTW_PHYS);
}

void produce(char * pipeFilePath, struct packet message) {
    int topicFd;
    topicFd = open(pipeFilePath, O_WRONLY);
    write(topicFd, message.body, message.size);
    close(topicFd);
}

void writeToTopic(struct packet message, char *topic) {
    char * completePath = malloc(sizeof(char) * (sizeof(ROOT_PATH) + sizeof(topic)));
    sprintf(completePath, "%s/%s", ROOT_PATH, topic);
    DIR * topicDirectory;
    struct dirent *topicFile;

    if ((topicDirectory = opendir(completePath)) == NULL) {
        if (errno == ENOENT)
            return;
    }

    while ((topicFile = readdir(topicDirectory)) != NULL) {
        if (topicFile->d_type == DT_FIFO) {
            char * pipeFilePath = malloc(sizeof(completePath) + sizeof(topicFile->d_name));
            sprintf(pipeFilePath, "%s/%s", completePath, topicFile->d_name);
            produce(pipeFilePath, message);
            free(pipeFilePath);
        }
    }

    free(completePath);
}

int definePacketType(char * clientInput) {
    char fixedHeader = clientInput[0];

    return (fixedHeader & 0xf0) >> 4;
}


struct packet parseInput(struct packet input) {
    struct packet response;
    bzero(&response, sizeof(response));

    char fixedHeader = input.body[0];
    
    int packetType = (fixedHeader & 0xf0) >> 4;

    if (packetType == CONNECT) {
        response.packetType = CONNACK;
        response.size = 4;
        char connack[4] = { 0x20, 0x02, 0x00, 0x00 };
        response.body = connack;
        response.body[response.size] = 0;
        return response;
    }


    if (packetType == SUBSCRIBE) {
        response.packetType = SUBACK;
        response.size = 5;
        char packetIdentifierMSB = input.body[3]; // TODO: mudar aqui caso pacote seja grande
        char packetIdentifierLSB = input.body[4];
      
        char suback[5] = { 0x90, 0x03, packetIdentifierLSB, packetIdentifierMSB, 0x00 };
        response.body = suback;
        response.body[response.size] = 0;

        return response;
    }

    if (packetType == PINGREQ) {
        response.packetType = PINGRESP;
        response.size = 2;
        char pingresp[2] = { 0xd0, 0x00 };

        response.body = pingresp;
        response.body[2] = 0;

        return response;
    }

    if (packetType == PUBLISH) {
        response.packetType = PUBLISH;
        return response;
    }
 
    return response;
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
    char clientInput[MAXLINE + 1];

    /* Armazena o tamanho da string lida do cliente */
    size_t n;

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
    eraseTopicFilesAndFolders();
    createRootDirectory();
   
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
        if ((connfd = accept(listenfd, NULL,
                  NULL)) == -1 ) {
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
            while ((n = read(connfd, clientInput, MAXLINE)) > 0) {
                pid_t subscriberPid;    
                struct packet input;
                struct packet response;
                bzero(&input, sizeof(input));
                input.body = clientInput;
                input.size = getMessageSize(clientInput);
                input.body[input.size] = 0;
                input.packetType = definePacketType(clientInput);

                if (input.packetType == DISCONNECT) {
                    break;
                }
                
                if ((fputs(clientInput,stdout)) == EOF) {
                    perror("fputs :( \n");
                    exit(6);
                }
                
                if (input.packetType == PUBLISH) {
                    writeToTopic(input, getProducerTopicName(input));
                }
                
                if (isSubscriber(input) && (subscriberPid = fork()) == 0) {
                    char * subscriberPipe = createTopicFile(getSubscriberTopicName(input));
                    int pipe_fd;
                    char pipeOutput[MAXLINE + 1];
                    while ((pipe_fd = open(subscriberPipe, O_RDONLY)) > 0) {
                        bzero(&pipeOutput, MAXLINE);
                        read(pipe_fd, pipeOutput, MAXLINE);
                        int inputSize = getMessageSize(pipeOutput);  
                        int messageType = definePacketType(pipeOutput);                  
                        pipeOutput[inputSize] = 0;
                        if (messageType == PUBLISH)
                            write(connfd, pipeOutput, inputSize); 
                        close(pipe_fd);
                    }
                }
                response = parseInput(input);
                write(connfd, response.body, response.size);
            }
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */

            // /* Após ter feito toda a troca de informação com o cliente,
            //  * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else {
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