/* Por Prof. Daniel Batista <batista@ime.usp.br>
 * Em 16/3/2022
 *
 * Um código simples de um servidor de eco a ser usado como base para
 * o EP1. Ele difere do outro código de exemplo porque está usando um
 * pipe para permitir que uma mesma mensagem enviada pelo primeiro
 * cliente conectado no servidor seja ecoada para outros clientes
 * conectados (pense bem nisso que esse código está fazendo e você vai
 * entender porque ele pode te ajudar na implementação de um broker
 * MQTT). Note que você não é obrigado a usar esse código como base. O
 * problema que esse código resolve pode ser resolvido de diversas
 * formas diferentes. Tudo que foi adicionado nesse código em relação
 * ao anterior está identificada com comentários com dois asteriscos.
 *
 * Ele recebe uma linha de um cliente e devolve a mesma linha.
 * Teste ele assim depois de compilar:
 *
 * ./mac0352-servidor-exemplo-ep1 8000
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
 *
 * Mas o real poder desse código vai ser exibido quando você conectar
 * três clientes. Conecte os três, escreva mensagens apenas no
 * terminal do primeiro e você entenderá.
 *
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

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

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

    /** Descritor de arquivo para o pipe **/
    int meu_pipe_fd[2];
    /** Nome do arquivo temporário que vai ser criado.
     ** TODO: isso é bem arriscado em termos de segurança. O ideal é
     ** que os nomes dos arquivos sejam criados com a função mkstemp e
     ** essas strings sejam templates para o mkstemp. **/
    char meu_pipe[2][27] = {"/tmp/temp.mac0352.1.XXXXXX", "/tmp/temp.mac0352.2.XXXXXX"};
    /** Para o loop de criação dos pipes **/
    int i;
    /** Variável que vai contar quantos clientes estão conectados.
     ** Necessário para saber se é o primeiro cliente ou não. **/
    int cliente;
    cliente = -2;

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

        /** Para identificar cada cliente. Se for o primeiro, o
         ** funcionamento vai continuar como sendo de um cliente de echo. Se
         ** não for, ele vai receber as mensagens do primeiro cliente **/
        cliente++;

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
            /* TODO: É esta parte do código que terá que ser modificada
             * para que este servidor consiga interpretar comandos MQTT  */
            /** Se for o primeiro cliente, continua funcionando assim
             ** como estava antes, com a diferença de que agora, tudo que for
             ** recebido vai ser colocado no pipe também (o novo write
             ** no fim do while). Note que estou considerando que
             ** terão 2 clientes conectados. O primeiro, que vai ser o cliente de echo
             ** de fato, e o outro que só vai receber as mensagens do primeiro.
             ** Por isso que foi adicionado um open, um write e um close abaixo.
             ** Obs.: seria necessário um tratamento para o caso do
             ** primeiro cliente sair. Isso está faltando aqui mas não é necessário
             ** para o próposito desse exemplo. Além disso, precisa
             ** revisar se esse unlink está no lugar certo. A depender
             ** do SO, pode não ser ok fazer o unlink logo depois do open. **/
            while ((n = read(connfd, recvline, MAXLINE)) > 0)
            {
                recvline[n] = 0;
                printf("[Cliente conectado no processo filho %d enviou:] ", getpid());
                int pkg_type = recvline[0] >> 4;
                int topicSize, messageSize, multiplier, currByte, remainingLenght, pipe_fd;
                char *topicName, *message, *pipe_name;
                fprintf(stderr, "Package: %d\n", pkg_type);
                for (int i = 0; i < n; i++)
                    fprintf(stderr, "%02x ", recvline[i]);
                fprintf(stderr, "\n");

                switch (pkg_type)
                {
                case 1:
                    fprintf(stderr, "CONNECT\n");
                    char resp1[4] = {0x20, 0x02, 0x00, 0x00};

                    if (write(connfd, resp1, 4) == -1)
                    {
                        perror("write 🙁 \n");
                        exit(6);
                    }
                    break;

                case 3:
                    fprintf(stderr, "PUBLISH\n");

                    multiplier = 1;
                    remainingLenght = 0;
                    currByte = 0;

                    do
                    {
                        currByte++;
                        remainingLenght += (recvline[currByte] & 127) * multiplier;
                        multiplier *= 128;
                    } while ((recvline[currByte] & 128) != 0);

                    fprintf(stderr, "Remaining Lenght: %d\n", remainingLenght);

                    topicSize = 0;
                    topicSize += (recvline[currByte + 1] >> 4) * 4096;
                    topicSize += (recvline[currByte + 1] & 15) * 256;
                    topicSize += (recvline[currByte + 2] >> 4) * 16;
                    topicSize += (recvline[currByte + 2] & 15);
                    fprintf(stderr, "Topic Size: %d\n", topicSize);

                    topicName = (char *)malloc(topicSize + 1);
                    memcpy(topicName, recvline + currByte + 3, topicSize);
                    topicName[topicSize] = '\0';

                    fprintf(stderr, "Topic: %s\n", topicName);

                    messageSize = remainingLenght - topicSize - 2;

                    fprintf(stderr, "Message Size: %d\n", messageSize);

                    message = (char *)malloc(messageSize + 1);
                    memcpy(message, recvline + currByte + 3 + topicSize, messageSize);
                    message[messageSize] = '\0';

                    fprintf(stderr, "Message: %s\n", message);

                    if (asprintf(&pipe_name, "/tmp/temp.mac0352.%s.XXXXXX", topicName) == -1)
                    {
                        perror("asprintf 🙁 \n");
                        exit(6);
                    }

                    if (mkfifo((const char *)pipe_name, 0666) == -1)
                    {
                        perror("mkfifo :(\n");
                    }

                    pipe_fd = open(pipe_name, O_RDWR);
                    unlink((const char *)pipe_name);
                    for (int i = 0; i < n; i++)
                        fprintf(stderr, "%02x ", recvline[i]);
                    fprintf(stderr, "Bytes written: %ld\n", write(pipe_fd, recvline, n));
                    close(pipe_fd);

                    break;

                case 8:
                    fprintf(stderr, "SUBSCRIBE\n");

                    char resp8[5] = {0x90, 0x03, 0x00, 0x01, 0x00};

                    if (write(connfd, resp8, 5) == -1)
                    {
                        perror("write 🙁 \n");
                        exit(6);
                    }

                    multiplier = 1;
                    remainingLenght = 0;
                    currByte = 0;

                    do
                    {
                        currByte++;
                        remainingLenght += (recvline[currByte] & 127) * multiplier;
                        multiplier *= 128;
                    } while ((recvline[currByte] & 128) != 0);

                    fprintf(stderr, "Remaining Lenght: %d\n", remainingLenght);

                    topicSize = 0;
                    topicSize += (recvline[currByte + 3] >> 4) * 4096;
                    topicSize += (recvline[currByte + 3] & 15) * 256;
                    topicSize += (recvline[currByte + 4] >> 4) * 16;
                    topicSize += (recvline[currByte + 4] & 15);
                    fprintf(stderr, "Size: %d\n", topicSize);

                    topicName = (char *)malloc(topicSize + 1);
                    memcpy(topicName, recvline + currByte + 5, topicSize);
                    topicName[topicSize] = '\0';

                    fprintf(stderr, "Topic: %s\n", topicName);

                    if (asprintf(&pipe_name, "/tmp/temp.mac0352.%s.XXXXXX", topicName) == -1)
                    {
                        perror("asprintf 🙁 \n");
                        exit(6);
                    }

                    if (mkfifo((const char *)pipe_name, 0666) == -1)
                    {
                        perror("mkfifo :(\n");
                    }

                    while (1)
                    {
                        fprintf(stderr, "Waiting for message...\n");
                        pipe_fd = open(pipe_name, O_RDWR);
                        unlink((const char *)pipe_name);
                        if ((n = read(pipe_fd, (void *)recvline, MAXLINE)) > 0)
                        {
                            fprintf(stderr, "Reading from pipe %ld bytes\n", n);
                            recvline[n] = 0;
                            write(connfd, recvline, n);
                        }
                    }
                    close(pipe_fd);

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
            /**** PROCESSO PAI ****/
            /* Se for o pai, a única coisa a ser feita é fechar o socket
             * connfd (ele é o socket do cliente específico que será tratado
             * pelo processo filho) */
            close(connfd);
    }
    exit(0);
}
