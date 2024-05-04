#include <sys/stat.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>

#include <message.h>
#include <mqtt.h>
#include <util.h>
#include <constants.h>

void createTmpFolder() {
    if (mkdir(TMP_DIR, 0777) == -1) {
        if (errno == EEXIST) {
            return;
        }

        perror("mkdir :(\n");
        exit(EXIT_FAILURE);
    };
}

void writeMessageToPipe(int connfd, Message response) {
    Packet output = messageToPacket(response);

    if (write(connfd, output.data, output.size) < 0) {
        perror("Erro ao escrever dados da mensagem. Fechando conexão.\n");
        exit(EXIT_FAILURE);
    }

    free(response.remaining);
    free(output.data);
}

int main(int argc, char **argv) {
    int listenfd, connfd;
    /* Informações sobre o ‘socket’ (endereço e porta) ficam nesta struct */
    struct sockaddr_in servaddr;
    pid_t childpid;
    Packet input;
    input.data = malloc((MAXLINE + 1) * sizeof(char));
    bzero(input.data, (MAXLINE + 1));

    if (argc != 2) {
        fprintf(stderr, "Uso: %s <Porta>\n", argv[0]);
        fprintf(stderr, "Vai rodar um servidor MQTT na porta <Porta> TCP\n");
        exit(EXIT_FAILURE);
    }

    /* Criação de um socket. É como se fosse um descritor de arquivo.
     * É possível fazer operações como read, write e close. Neste caso o
     * socket criado é um socket IPv4 (por causa do AF_INET), que vai
     * usar TCP (por causa do SOCK_STREAM), já que o MQTT funciona sobre
     * TCP, e será usado para uma aplicação convencional sobre a Internet
     * (por causa do número 0) */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(EXIT_FAILURE);
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
    if (bind(listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(EXIT_FAILURE);
    }

    /* Como este código é o código de um servidor, o socket será um
     * socket passivo. Para isto é necessário chamar a função listen
     * que define que este é um socket de servidor que ficará esperando
     * por conexões nos endereços definidos na função bind. */
    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(EXIT_FAILURE);
    }

    /* Cria a pasta temporária em que 
     * serão armazenados os pipes */
    createTmpFolder();

    /* Define rotinas de clean up para serem executadas
     * quando o processo estiver finalizando */
    setSignIntAction();

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n", argv[1]);
    printf("[Para finalizar, pressione CTRL+C ou rode um kill ou killall]\n");

    while (1) {
        /* O socket inicial que foi criado é o socket que vai aguardar
         * pela conexão na porta especificada. Mas pode ser que existam
         * diversos clientes conectando no servidor. Por isso deve-se
         * utilizar a função accept. Esta função vai retirar uma conexão
         * da fila de conexões que foram aceitas no socket listenfd e
         * vai criar um socket específico para esta conexão. O descritor
         * deste novo socket é o retorno da função accept. */
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1) {
            perror("accept :(\n");
            exit(EXIT_FAILURE);
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
        if ((childpid = fork()) == 0) {
            /**** PROCESSO FILHO ****/
            printf("\n[Uma conexão aberta]\n");
            /* Já que está no processo filho, não precisa mais do socket
             * listenfd. Só o processo pai precisa deste socket. */
            close(listenfd);

            /* Processo filho não precisa executar
             * as rotinas de clean up */
            resetSignIntAction();

            while ((input.size = read(connfd, input.data, MAXLINE)) > 0) {
                printf("\n[Cliente conectado no processo filho %d enviou uma mensagem]", getpid());
                Message message = packetToMessage(input);
                Message response = readMessage(message, connfd);

                if (response.packetType == RESERVED)
                    break;

                writeMessageToPipe(connfd, response);
            }

            if (input.size < 0) {
                perror("Erro ao ler dados da mensagem. Fechando conexão.\n");
                exit(EXIT_FAILURE);
            }

            /* Caso tenha uma thread de subscribe rodando, a finaliza */
            terminateThread();

            /* Após ter feito toda a troca de informação com o cliente,
             * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(EXIT_SUCCESS);
        } else {
            /**** PROCESSO PAI ****/

            /* Evita que os processos filhos
            continuem vivos após o término
            de sua execução */
            signal(SIGCHLD, SIG_IGN);
        }

        /* Se for o pai, a única coisa a ser feita é fechar o socket
         * connfd (ele é o socket do cliente específico que será tratado
         * pelo processo filho) */
        close(connfd);
    }
    free(input.data);
    exit(EXIT_SUCCESS);
}
