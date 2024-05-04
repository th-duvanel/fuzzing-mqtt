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
#include <dirent.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

/* Algoritmo de hash djb2 de Dan Bernstein */
unsigned long hash(unsigned char* topic) {
    unsigned long hash = 5381;
    int c;

    while ((c = *topic++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

/* Recebe dois bytes a e b (números hexadecimais)
 * e retorna o valor inteiro desses bytes concatenados
 * na ordem a-b.
*/
int getIntValueFromBytes(char a, char b) {
    return (((a & 0xff) << 8) | (b & 0xff));
}

/* Recebe um ponteiro para o início do nome do tópico
 * e a largura do nome. Retorna uma string com o caminho
 * completo até o diretório 
 */
char* getDirPathString(unsigned char* topic, int topicLength) {
    char* basePath = "/tmp/";
    unsigned char* topicName = malloc(topicLength);
    memcpy(topicName, topic, topicLength);
    char* path = malloc(26);
    sprintf(path, "%s%020lu/", basePath, hash(topicName));
    mkdir(path, 0777);
    return path;
}

void sendPublishMessage(int connfd, unsigned char* message, char* topic, unsigned char topicLenb1, unsigned char topicLenb2) {
    char totalMsgLength = 2 + strlen((char*)message) + strlen(topic);
    int topicLength = strlen(topic);

    char* publish = malloc(2 + totalMsgLength);
    char headPublish[4] = { 0x30, totalMsgLength, topicLenb1, topicLenb2 };
    memcpy(publish, headPublish, 4);
    memcpy(publish + 4, topic, topicLength);
    memcpy(publish + 4 + topicLength, (char*) message, strlen((char*)message));
    write(connfd, publish, 2 + totalMsgLength);
    free(publish);
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

    DIR * dirp;
    struct dirent * entry;
    int subCount = 0, pipe;
    char* path;

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

            /* Cria variável para guardar o valor do tipo de pacote.
             * Essa variável é inicializada com o valor 0 (reservado)
             * e deve ser alterada após a leitura do primeiro byte.
             */
            int packetType = 0, topicLength;
            while ((n=read(connfd, recvline, MAXLINE)) > 0) {
                recvline[n]=0;
                packetType = recvline[0]>>4;

                switch (packetType) {
                    case 1:
                        /* CONNECT */
                        if (recvline[4] == 'M' && recvline[5] == 'Q' && recvline[6] == 'T' && recvline[7] == 'T') {
                            /* Se o servidor recebeu uma menssagem do tipo CONNECT válida,
                             * então retorna uma mensagem do tipo CONNACK
                             */
                            char ans[4] = { 0x20, 0x02, 0x00, 0x00 };
                            write(connfd, ans, 4);
                        }
                        break;
                    case 3:
                        /* PUBLISH */
                        /* O tamanho da mensagem enviada aos subscribers é igual ao 
                        * tamanho total da mensagem subtraído  do tamanho do tópico
                        * e dos 2 bytes ocupados pelo topic length (que indica o
                        * tamanho do nome do tópico em bytes).
                        * Organização: MessageLen - TopicLen (2 bytes) - Topic - Message
                        */
                        int totalMsgLength = recvline[1];
                        topicLength = getIntValueFromBytes(recvline[2], recvline[3]);

                        int messageLength = totalMsgLength - topicLength - 2;
                        unsigned char* message = malloc(messageLength);
                        memcpy(message, recvline + 4 + topicLength, messageLength);

                        path = getDirPathString(recvline + 4, topicLength);

                        dirp = opendir(path);
                        while (dirp != NULL && (entry = readdir(dirp)) != NULL) {
                            char* pathToFile = malloc(30);
                            sprintf(pathToFile, "%s%s", path, entry->d_name);
                            
                            pipe = open(pathToFile,O_RDWR);
                            unlink((const char *) path);
                            write(pipe, message, messageLength);
                            free(pathToFile);
                        }

                        closedir(dirp);
                        free(message);
                        free(path);
                        break;
                    case 8:
                        /* SUBSCRIBE */
                        unsigned char topicLenb1 = recvline[4];
                        unsigned char topicLenb2 = recvline[5];
                        topicLength = getIntValueFromBytes(topicLenb1, topicLenb2);
                        path = getDirPathString(recvline + 6, topicLength);
                        char* topicName = malloc(topicLength);
                        memcpy(topicName, recvline + 6, topicLength);

                        /* Retorna uma mensagem do tipo SUBACK */
                        char suback[5] = { 0x90, 0x03, recvline[2], recvline[3], 0x00 };
                        write(connfd, suback, 5);

                        /* Encontra a "ordem" do subscriber */
                        dirp = opendir(path);
                        while (dirp != NULL && (entry = readdir(dirp)) != NULL) subCount++;
                        closedir(dirp);

                        /* Adiciona um pipe para o novo subscriber */
                        char* pipeName = malloc(30);
                        sprintf(pipeName, "%s%04d", path, subCount);
                        if (mkfifo((const char*) pipeName, 0644) == -1) {
                            perror("mkfifo :(\n");
                        }

                        /* Subscriber começa a receber os dados de seu pipe */
                        pipe = open((const char*) pipeName,O_RDWR);
                        while ((n=read(pipe, recvline, MAXLINE)) > 0) {
                            recvline[n]=0;
                            /* Retorna uma mensagem do tipo PUBLISH */
                            sendPublishMessage(connfd, recvline, topicName, topicLenb1, topicLenb2);
                        }
                        unlink((const char *) pipeName);
                        free(topicName);
                        free(path);
                        free(pipeName);
                        close(pipe);
                        
                        break;
                    case 12:
                        /* PING */
                        /*Se recebeu um PINGREQ, retorna um PINGRESP */
                        char pingresp[2] = { 0xd0, 0x00 };
                        write(connfd, pingresp, 2);

                        break;
                    default:

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
        else {
            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            close(connfd);
        }
    }
    exit(0);
}
/*mosquitto_sub -p 8883 -v -t 'test/topic2'
mosquitto_pub -t 'test/topic2' -m 'olhos'
rm -rf /tmp/temp.mac0352.13895175586750516005.0000000000 */