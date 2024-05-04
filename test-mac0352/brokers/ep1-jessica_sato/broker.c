/** ******************************************************************
 ** EP1 - REDES - MQTT BROKER
 ** 2022-1
 **
 ** ALUNA: Jessica Yumi Nakano Sato
 ** NUSP : 11795294
 **
 ** Codigo modificado para se comportar como um mqtt broker simples.
 ********************************************************************/

/* Código original por Prof. Daniel Batista <batista@ime.usp.br>
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
#include <string.h>
#include <dirent.h>
#include <pthread.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

/* Estrutura thread_arg:
 * Usada nas threads, carrega o endereco do socket em connfd e o endereco*
 * da pipe que serao utilizados nas threads                              */
typedef struct{
    int* connfd;
    char* pathname;
}thread_arg;

/* Funcao subscribe:
 *  Funcao auxiliar para melhor visualizacao do pacote recebido          */
void print_hexa_bytes(char recvline[MAXLINE+1], int n){
    for (int i =0; i <n+1; i++) printf("%02x ", recvline[i]);
    printf("\n");
}

/* Funcao thread_socket:
 *  É utilizada durante o subscribe.                                     *
 *  Ela é responsavel por ler constante mente o socket, para reconhecer, *
 *  caso seja enviado, um novo pacote do cliente sub                     */
void* thread_socket(void* arg){ 
    thread_arg* pack = arg;
    int n;
    unsigned char recvline[MAXLINE+1];
    int connfd = *pack->connfd;
    while((n=read(connfd, recvline, MAXLINE)) > 0){        
        if(recvline[0] == 0xc0){ // PING
            printf("=>Packet de ping request recebido\n");                   
            char devolucao[2] = {0xd0, 0x00};
            write(connfd, devolucao, 2);
            printf("<=Packet de ping response enviado\n");
        }
        else if (recvline[0] == 0xe0){ //DISC
            printf("=>Packet de disconnect recebido\n");            
            break;
        }
	else{
            printf("=>Packet de comando desconhecido recebido\n");
        }
    }
    close(open(pack->pathname, O_WRONLY | O_NONBLOCK));
    
    return NULL;
}

/* Funcao thread_pipe:
 *  É utilizada durante o subscribe.                                  *
 *  Ela é responsavel por passar o que esta no arquivo da pipe para o *
 *  socket do cliente subscriber.                                     */
void* thread_pipe(void* arg){ //leitura da pipe no sub
    thread_arg* pack = arg;
    int n, i;
    unsigned char recvline[MAXLINE+1];
    int fd; 
    fd = open(pack->pathname, O_RDONLY);
    int connfd = *pack->connfd;
    while ((n = read(fd, recvline, MAXLINE)) > 0){
        unsigned int remaining_length = 0;
        int multiplier = 1;
        int mult_times=0;
        do{
            mult_times ++;
            remaining_length += (recvline[mult_times] & 127) * multiplier;
            multiplier *= 128;
           
            if (mult_times > 4){
                printf("Packet maior do que o possível\n");
                return NULL;
            }
        }while((recvline[mult_times] & 128) != 0);
        
        if (write(connfd, recvline, remaining_length + 1 + mult_times) < 0) printf("nao passou pra frente\n");
        close(fd);
        fd = open(pack->pathname, O_RDONLY);
    }
    close(fd);
    unlink((const char*) pack->pathname);
    return NULL;
}

/* Funcao check_connect:
 *  É utilizada ao reconhecer um packet contendo o comando de connect. *
 *  Ela ira fazer uma breve checagem, conferindo apenas se o pacote de *
 *  connect tem nome do protocolo correto.                             */
char check_connect(char recvline[MAXLINE+1]){
    if (recvline[2] != 0x00 || recvline[3] != 0x04){
        printf("Incorrect protocol name length.\n");
        return 0x01;
    }
    if (recvline[4] != 'M' || recvline[5] != 'Q' || recvline[6] != 'T' || recvline[7] != 'T'){
        printf("Incorrect protocol name.\n");
        return 0x01;
    }
    return 0x00;
}

/* Funcao publish:
 *  É utilizada ao reconhecer um packet contendo o comando de publish.*
 *  Ela irá escrever nos arquivos referentes ao topico.               * 
 *  (arquivos do tipo /tmp/temp.mac0352.<topico> )                    */
void publish (char recvline[MAXLINE+1]){
    unsigned int remaining_length = 0;

    int multiplier = 1;
    int mult_times = 0;
    do{
        mult_times ++;
        remaining_length += (recvline[mult_times] & 127) * multiplier;
        multiplier *= 128;
        
        if (mult_times > 4){
            printf("Packet maior do que o possível\n");
            return;
        }
    }while((recvline[mult_times] & 128) != 0);
    
    unsigned int topic_length = recvline[mult_times+2] + recvline[mult_times+1] * 256;

    char* topic = malloc(sizeof(char) * (topic_length + 1));
    for (int i = 0; i < topic_length ; i++){
        topic[i] = recvline[i+mult_times+3];
    }
    topic[topic_length] = 0;

    
    char* pipe_template = malloc(sizeof(char) * (topic_length + 15));
    strncpy(pipe_template, "temp.mac0352.", 14);
    strcat(pipe_template, topic);
    strcat(pipe_template, ".");
    
    DIR* d;
    struct dirent* dir;
    d = opendir("/tmp");
    if (d != NULL){
        /* Esse laco passara pelo nome dos arquivos na pasta temp */
        while((dir = readdir(d)) != NULL){
            int found_topic = 1;
            /* Esse laco reconhecera se o arquivo tem o nome procurado */
            for (int i = 0; found_topic && (i < 14+topic_length); i++){
                if(pipe_template[i] != dir->d_name[i]){
                    found_topic = 0;
                }
            }
            /* Sendo um arquivo do topico, faremos a escrita */
            if (found_topic){
                char* pipe_name = malloc(sizeof(char) * (5 + strlen(dir->d_name)));
                strncpy(pipe_name, "/tmp/", 5);
                strcat(pipe_name, dir->d_name);
                int fd = open(pipe_name, O_WRONLY | O_NONBLOCK);
                if (write(fd, recvline, remaining_length + 1 + mult_times) == -1) printf("write :(\n");
                close(fd);
            } 
        }
    }
    free(topic);
    free(pipe_template);
    return;
}
/* Funcao subscribe:
 *  É utilizada ao reconhecer um packet contendo o comando de subscribe.      *
 *  Ela irá criar as threads e o arquivo especial utilizado na thread da pipe */ 
void subscribe(char recvline[MAXLINE+1], int connfd, int pid){
    unsigned int remaining_length;
    int multiplier = 1;
    int mult_times = 0;
    do{
        mult_times ++;
        remaining_length += (recvline[mult_times] & 127) * multiplier;
        multiplier *= 128;
        
        if (mult_times > 4){
            printf("Packet maior do que o possível\n");
            return;
        }
    }while((recvline[mult_times] & 128) != 0);
    
    unsigned int topic_length = (int) recvline[mult_times+3] + (int) recvline[mult_times+2] * 256;
   
    char* topic = malloc(sizeof(char) * (topic_length + 1));
    for (int i = 0; i < topic_length ; i++){
        topic[i] = recvline[i+5+mult_times];
    }    
    topic[topic_length] = '\0';
    
    char* pid_char = malloc(sizeof(char) * 10);
    sprintf(pid_char, "%d", pid);
    
    /* Criacao do arquivo com nome /tmp/temp.mac0352.<topico>.<pid> */
    char* pathname = malloc(sizeof(char) * (strlen(pid_char) + topic_length + 20));
    strncpy(pathname, "/tmp/temp.mac0352.", 18);
    strcat(pathname, topic);
    strcat(pathname, ".");
    strcat(pathname, pid_char);
    
    if (mkfifo((const char *) pathname,0644) == -1) {
        perror("mkfifo :(\n");
    }
    
    /* Criacao das threads */
    thread_arg* ta = malloc(sizeof * ta);
    ta->connfd = &connfd;
    ta->pathname = pathname;        
    
    pthread_t t_socket, t_pipe;
    
    pthread_create(&t_socket, NULL, thread_socket, ta);
    pthread_create(&t_pipe, NULL, thread_pipe, ta);
    pthread_join(t_socket, NULL);
    pthread_join(t_pipe, NULL);

    unlink((const char*) pathname);
    free(topic);
    free(pid_char);
    free(pathname);
    free(ta);
    return;
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
                if (recvline[0] == 0x10){       //CONNECT
                    printf("=>Packet de connect recebido\n");
                    char last_byte = check_connect(recvline);
                    char devolucao[4] = {0x20, 0x02, 0x01, last_byte};
                    write(connfd, devolucao, 4);
                    printf("<=Packet de connack enviado\n");
                }
                else if (recvline[0] == 0x82){ //SUB
                    printf("=>Packet de subscribe recebido\n"); 
                    
                    /*  Aqui estamos calculando onde está o MSB e LSB  * 
                     * para enviar no suback                           */  
                    int mult_times = 0;
                    do{
                        mult_times ++;
                        if (mult_times > 4){
                            printf("Packet maior do que o possível\n");
                            break;
                        }
                    }while((recvline[mult_times] & 128) != 0);  
                                  
                    char devolucao[5] = {0x90, 0x03, recvline[1+mult_times], recvline[2+mult_times], 0x00};
                    write(connfd, devolucao, 5);
                    printf("<=Packet de suback enviado\n");
                    subscribe(recvline, connfd, getpid());
                    break;
                }
                else if (recvline[0] == 0x30){ //PUB
                    printf("=>Packet de publish recebido\n");                   
                    publish(recvline);
                }
                else if(recvline[0] == 0xc0){  // PING
                    printf("=>Packet de ping request recebido\n");                   
                    char devolucao[2] = {0xd0, 0x00};
                    write(connfd, devolucao, 2);
                    printf("<=Packet de ping response enviado\n");
                }
                else if (recvline[0] == 0xe0){ //DISC
                    printf("=>Packet de disconnect recebido\n");                   
                }
                else{
                    printf("=>Packet de comando desconhecido recebido\n");                   
                }
                /* Packet do disconnect do pub as vezes vem grudado: */
                if((n > (int) recvline[1] + 2) && recvline[(int) recvline[1] + 2] == 0xe0){
                    printf("=>Packet de disconnect recebido\n");
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
};
