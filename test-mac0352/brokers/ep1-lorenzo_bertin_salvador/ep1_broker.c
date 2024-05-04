/*
    Código MAC0352 EP1 - Broker MQTT

    Lorenzo Bertin Salvador 11795356   


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
#include <sys/stat.h>
#include <fcntl.h>

#include <sys/time.h>

#define LISTENQ 1
#define MAXLINE 4096

#define MAXCLIENTS 30000 // máximo de clientes que podem passar pelo servidor (não simultâneo).
#define REGISTERLENGTH 40 // tamanho do nome do arquivo de registro.
#define FDLENGTH 40 // tamanho do nome do file descriptor dos clientes.

#define COOLDOWN 3 // intervalo de tempo que os descritores são checados para buscar por comandos ou mensagens.


/*
    Estrutura do cliente.
*/

typedef struct meu_cliente{
    int number; // número do cliente
    char* Id; // id do cliente
    int qtd_topics; // quantidade de tópicos que um cliente sub está inscrito
    int fd; // file descriptor do cliente (-1 se cliente não for sub)
} meu_cliente;


/*
    Função que recebe um inteiro n, o converte para a base binárias
    e devolve uma lista onde cada elemento dela é um dos bits de n convertido.
*/

int* decimal_to_binary(int n){
    int binario[50];
    int i = 0;
    while (n > 0 ) {
        binario[i] = n % 2;
        n = n / 2;
        i++;
    }
    int tam = 0;
    int *resp = malloc(sizeof(int)*8);
    if (resp == NULL) perror("Erro na alocação de resp em decimal_to_binary");
    for (int j = 0; j < 8-i; j = j + 1){
        resp[tam] = 0;
        tam++;
    }
    for (int j = i-1; j >= 0; j--){
        resp[tam] = binario[j];
        tam++;
    }
    return resp;

}

/*
    Função que recebe uma lista de 8 inteiros representando 1 byte
    e o converte para um decimal
*/

int binary_to_decimal(int binary[]){
    int decimal = 0;
    for (int i = 7; i >= 0; i = i -1){
        decimal = decimal + pow(2,7-i)*binary[i];
    }
    return decimal;
}

/*
    Função que recebe uma mensagem (como inteiros) e a transforma em uma lista de bytes,
    onde cada byte é uma lista de 8 inteiros.
*/

int **dump_bytes(int bytes[],int size){
    int **dump = malloc(sizeof(int*)*size);
    if (dump == NULL) perror("Erro na alocação de dump em dump_bytes");
    for (int i = 0; i < size;i++){        
        int *bin = decimal_to_binary(bytes[i]);
        dump[i] = malloc(sizeof(int)*8);
        if (dump[i] == NULL) perror("Erro na alocação de dump[i] em dump_bytes");
        for (int j = 0; j < 8;j++){
            dump[i][j] = bin[j];
        }
        free(bin); // libera o que foi alocado em decimal_to_binary
    }
    return dump;    
}

/*
    Função que recebe uma lista de números em binários e os imprime na saída padrão.
*/

void print_message(int** dump,int message_size){

    for (int i = 0; i < message_size;i++){
        printf("Byte %d:",i+1);
        for (int j = 0; j < 8;j++){
            printf("%d",dump[i][j]);
        }
        printf(" = %c %d \n",binary_to_decimal(dump[i]),binary_to_decimal(dump[i]));
    }
    return;
}

/*
    Função que libera o que foi alocado em dump_bytes()
*/
void free_dump(int ** dump,int bytes_size){

    for (int i =0; i < bytes_size;i++){
        free(dump[i]);
    }
    free(dump);
}

/*
    Função que compara 2 arrays que representam 1 byte. Se um dos bits puder assumir qualquer valor, seu valor é -1.
*/
int equalArrays(int a[],int b[],int size){
    for (int i = 0; i < size;i++){
        if (a[i] != b[i] && a[i] != -1 && b[i] != -1) return 0;
    }
    return 1;
}

/*
    Função que envia CONNACK para o cliente.
*/

void send_CONNACK(int connfd,int clean_session,meu_cliente client){

    char resposta[5];
    resposta[0] = (char) 32;
    resposta[1] = (char) 2;
    if (clean_session == 1) resposta[2] = (char) 0;
    else{
        if (strcmp(client.Id,"") != 0) resposta[2] = (char) 1;
        else resposta[2] = (char) 0;
    }
    resposta[3] = (char) 0;
    write(connfd, resposta, 4);
    printf("Servidor respondeu CONNACK para o cliente %s(%d)\n",client.Id,client.number);
    return;
}


/*
    Função que lida com o recebimento da mensagem CONNECT.
*/
meu_cliente  received_CONNECT(int connfd,int  **dump,int size,meu_cliente client){
    
    int start = 3;
    int protocol_name_length = binary_to_decimal(dump[start]);
    start = start + protocol_name_length+1;
    start++;
    int *byte8 = dump[start];
    int clean_session = byte8[6];
    start++;
    start++;
    start = start + 2;
    int payload_length = binary_to_decimal(dump[start]);
    start++;
    client.Id = malloc(sizeof(char)*(payload_length+1));     
    if (client.Id == NULL) perror("Erro na alocação de client.id");
    memset(client.Id,0,payload_length);
    for (int i = 0; i < payload_length;i++){
        char c = (char)binary_to_decimal(dump[start+i]);
        strncat(client.Id,&c,1);
    }
    client.Id[payload_length] = 0;
    printf("Servidor recebeu CONNECT de cliente %s(%d)\n",client.Id,client.number);
    send_CONNACK(connfd,clean_session,client);

    free_dump(dump,size);
    return client;
}

/*
    Função que envia PUBLISH para os clientes inscritos em determinado tópico.
*/

void send_PUBLISH(meu_cliente client,int **dump,int size,int remaining_length,char pipes[][FDLENGTH],int pipes_fd[],char *topic,char register_pipe[REGISTERLENGTH]){

    // Prepara payload para enviar para os clientes inscritos no tópico
    char* payload = malloc(sizeof(char) * size);
    payload[0] = (char) 48;
    payload[1] = (char) remaining_length;
    for (int i = 2; i < 2+remaining_length;i++){
        payload[i] = (char) binary_to_decimal(dump[i]);
    }

    // Lê no registro quais clientes estão inscritos no tópico e escreve a mensagem em seus respectivos file descriptors
    FILE *fp;
    fp = fopen(register_pipe,"r");
    if (NULL == fp) perror("Erro na leitura\n");


    // Primeiro é necessário descobrir quantos caractéres têm no arquivo.
    char infos[2][10];
    int idx = 0;
    strcpy(infos[0],"");
    strcpy(infos[1],"");
    while(1){
        char c = fgetc(fp);
        if (feof(fp)) break;
        if (c == '/'){
            idx++;
            continue;
        }
        if (c == ';') break; 
        strncat(infos[idx],&c,1); 
    }
    int remain = atoi(infos[0]);
    idx = 0;
    // Lê o resto do arquivo em busca de algum cliente inscrito no tópico.
    char topico[100];
    char cliente[4];
    strcpy(topico,"");
    strcpy(cliente,"");

    for (int i = 0; i < remain;i++){
        char c = fgetc(fp);
        if (c == ';'){
            int cl = atoi(cliente);
            if (strcmp(topico,topic) == 0){
                pipes_fd[cl] = open(pipes[cl],O_WRONLY);
                write(pipes_fd[cl],payload,remaining_length+2);    
                close(pipes_fd[cl]);               
            }
            strcpy(topico,"");
            strcpy(cliente,"");
            idx = 0;
            continue;    
        } 
        if (c == ','){
            idx++;
            continue;  
        } 

        if (idx == 0) strncat(cliente,&c,1);
        else strncat(topico,&c,1);

    }

    fclose(fp);
    free(payload);
    return;
}


/*
    Função que lida com o recebimento da mensagem DISCONNECT.
*/

meu_cliente  received_DISCONNECT(int connfd,int  **dump,int size,meu_cliente client){
    printf("Servidor recebeu DISCONNECT de cliente %s(%d)\n",client.Id,client.number);
    free_dump(dump,size);
    client.number = -1;
    return client;
}

/*
    Função que lida com o recebimento de uma mensagem desconhecida.
*/

meu_cliente  received_UNKNOWN(int connfd,int  **dump,int size,meu_cliente client){
    printf("Mensagem não reconhecida de cliente %s(%d)\n",client.Id,client.number);
    printf("Identificador da mensagem: %d",binary_to_decimal(dump[0]));
    free_dump(dump,size);
    return client;
}


/*
    Função que lida com o recebimento da mensagem PUBLISH.
*/

meu_cliente received_PUBLISH(int connfd,int **dump,int size,meu_cliente client,char pipes[][FDLENGTH],int pipes_fd[],char register_pipe[REGISTERLENGTH]){
    printf("Servidor recebeu PUBLISH de cliente %s(%d)\n",client.Id,client.number);

    int remaining_length = binary_to_decimal(dump[1]);
    
    int start = 2;
    start++;
    int lsb_length = binary_to_decimal(dump[start]);
    start++;
    char *topic = malloc(sizeof(char)*(lsb_length+1));
    memset(topic,0,lsb_length);
    for (int i = 0; i < lsb_length;i++){
        char c = (char) binary_to_decimal(dump[start+i]);
        strncat(topic,&c,1);
    }
    topic[lsb_length] = 0;

    start = start + lsb_length;
    int app_message_length = remaining_length - lsb_length - 2;
    char *app_message = malloc(sizeof(char)*(app_message_length+1));
    memset(app_message,0,app_message_length);
    for (int i = 0; i < app_message_length;i++){
        char c = (char) binary_to_decimal(dump[start+i]);
        strncat(app_message,&c,1);
    }
    app_message[app_message_length] = 0;
    start = start + app_message_length;
    printf("Cliente %d está publicando no tópico %s: %s\n",client.number,topic,app_message);


    send_PUBLISH(client,dump,size,remaining_length,pipes,pipes_fd,topic,register_pipe);

    int disc = 0;
    if (start < size && binary_to_decimal(dump[start]) == 224) disc = 1;

    free(topic);
    free(app_message);
    if (disc == 1) return received_DISCONNECT(connfd,dump,size,client);
    free_dump(dump,size);
    return client;
}

/*
    Função que envia SUBACK para o cliente.
*/

void send_SUBACK(int connfd,int packet_identifier_msb, int packet_identifier_lsb,int qos,meu_cliente client){

    char resposta[6];
    resposta[0] = (char) 144;
    resposta[1] = (char) 3;
    resposta[2] = (char) packet_identifier_msb;
    resposta[3] = (char) packet_identifier_lsb;
    if (qos == 0)
        resposta[4] = (char) 0;
    else if (qos == 1)
        resposta[4] = (char) 1;
    else
        resposta[4] = (char) 2;

    write(connfd,resposta,5);
    printf("Servidor respondeu SUBACK para o cliente %s(%d)\n",client.Id,client.number);
    return;

}


/*
    Função que lida com o recebimento da mensagem SUBSCRIBE.
*/


meu_cliente  received_SUBSCRIBE(int connfd,int **dump,int size,meu_cliente client,char pipes[][FDLENGTH],int pipes_fd[],char register_pipe[REGISTERLENGTH]){
    printf("Servidor recebeu SUBSCRIBE de %s\n",client.Id);
    int start = 1;

    // Um SUBSCRIBE pode ter mais de um tópico.
    while (start < size){

        start++;
        int packet_identifier_msb = binary_to_decimal(dump[start]);
        start++;
        int packet_identifier_lsb = binary_to_decimal(dump[start]);
        start++;
        start++;
        int lsb_length = binary_to_decimal(dump[start]);
        start++;
        client.qtd_topics++;
        char *topic = malloc(sizeof(char)*(lsb_length+1));
        if (topic == NULL) perror("Erro na alocação de tópico");
        memset(topic,0,lsb_length);
        for (int i = 0; i < lsb_length;i++){
            char c = (char) binary_to_decimal(dump[start+i]);
            strncat(topic ,&c,1);
        }
        topic[lsb_length] = 0;
        start = start + lsb_length;
        int requested_qos = binary_to_decimal(dump[start]);
        start = start + 2;

        // Responde cliente com o SUBACK.
        send_SUBACK(connfd,packet_identifier_msb,packet_identifier_lsb,requested_qos,client);


        printf("Cliente %d está inscrito no tópico %s (%d)\n",client.number,topic,client.qtd_topics);

        // Operação para determinar quantos dígitos tem o número do cliente (útil para o arquivo de registro)
        int digitos = 0;
        int num = client.number;
        do {
            num /= 10;
            ++digitos;
        } while (num != 0);

        // Lê arquivo de registro para armazenar as informações presentes até o momento.
        FILE *fp;
        fp = fopen(register_pipe,"r");
        if (NULL == fp) perror("Erro na leitura\n");
        char infos[2][50];
        int idx = 0;
        strcpy(infos[0],"");
        strcpy(infos[1],"");
        while(1){
            char c = fgetc(fp);
            if (feof(fp)) break;
            if (c == '/'){
                idx++;
                continue;
            }
            if (c == ';') break; 
            strncat(infos[idx],&c,1); 
        }
        int remain = atoi(infos[0]);
        int qtd_registers = atoi(infos[1]);
        idx = 0;
        char *rest = malloc(sizeof(char)*(remain+1));
        if (rest == NULL) perror("Erro na alocação de rest");
        memset(rest,0,remain);
        strcpy(rest,"");
        for (int i = 0; i < remain;i++){
            char c = fgetc(fp);
            if (c != EOF) strncat(rest,&c,1);
        }
        fclose(fp);
        rest[remain] = 0;
    
        // Adiciona o novo tópico no arquivo de registro, concatenando com os dados que já estavam lá.
        fp = fopen(register_pipe, "w");
        if (remain == 0) fprintf(fp,"%d/%d;%d,%s;",(remain+lsb_length+digitos+2),(qtd_registers+1),client.number,topic);
        else fprintf(fp,"%d/%d;%d,%s;%s",(remain+lsb_length+digitos+2),(qtd_registers+1),client.number,topic,rest);
        fclose(fp);

        free(rest); 
        free(topic);

    }

    // Abre o file descriptor do cliente para leitura.
    int i = client.number;
    pipes_fd[i] = open(pipes[i],O_RDONLY);
    client.fd = pipes_fd[i];

    free_dump(dump,size);

    return client;
}


/*
    Função que envia PINGRESP para o cliente.
*/

void send_PINGRESP(int connfd,meu_cliente client){
    char resposta[3];
    resposta[0] = (char) 208;
    resposta[1] = (char) 0;
    write(connfd,resposta,2);
    printf("Servidor respondeu PINGRESP para o cliente %s(%d)\n",client.Id,client.number);    
    return;
}


/*
    Função que lida com o recebimento da mensagem PINGREQ.
*/


meu_cliente  received_PINGREQ(int connfd,int **dump,int size,meu_cliente client){
    printf("Servidor recebeu PINGREQ de cliente %s(%d)\n",client.Id,client.number);
    send_PINGRESP(connfd,client);
    free_dump(dump,size);
    return client;
}



/*
    Função que recebe as mensagens enviadas pelo cliente e as interpreta. 
*/
meu_cliente interpret_message(int connfd,int full_bytes[],int size,meu_cliente client,char pipes[][FDLENGTH],int pipes_fd[],char register_pipe[REGISTERLENGTH]){
    int **dump = dump_bytes(full_bytes,size);
    // print_message(dump,size); // <-- para ver byte por byte da mensagem.

    int connect_bits[] = {0,0,0,1,0,0,0,0};
    int publish_bits[] = {0,0,1,1,-1,-1,-1,-1}; // -1 representa que podem assumir qualquer valor (flags).
    int subscribe_bits[] = {1,0,0,0,0,0,1,0};
    int pingreq_bits[] = {1,1,0,0,0,0,0,0};
    int disconnect_bits[] = {1,1,1,0,0,0,0,0};

    if (size > 0 && equalArrays(dump[0],connect_bits, 8))
        return received_CONNECT(connfd,dump,size,client);
        
    else if (size > 0 && equalArrays(dump[0],publish_bits,8))
        return received_PUBLISH(connfd,dump,size,client,pipes,pipes_fd,register_pipe);
    
    else if (size > 0 && (equalArrays(dump[0],subscribe_bits,8)))
        return received_SUBSCRIBE(connfd,dump,size,client,pipes,pipes_fd,register_pipe);

    else if (size > 0 && equalArrays(dump[0],pingreq_bits,8))
        return received_PINGREQ(connfd,dump,size,client);

    else if (size > 0 && (equalArrays(dump[0],disconnect_bits,8) == 1))
        return received_DISCONNECT(connfd,dump,size,client);

    else
        return received_UNKNOWN(connfd,dump,size,client);
    
}


//* A função a seguir não é de minha autoria, ela consta de exemplo no site a seguir

// Fonte: https://www.gnu.org/software/libc/manual/html_node/Waiting-for-I_002fO.html

/*
    Função que recebe um file descriptor fd e tempo t e checa se há conteúdo em fd a cada t segundos
 */
int input_timeout (int filedes, unsigned int seconds)
{
  fd_set set;
  struct timeval timeout;


  /* Initialize the file descriptor set. */
  FD_ZERO (&set);
  FD_SET (filedes, &set);

  /* Initialize the timeout data structure. */
  timeout.tv_sec = seconds;
  timeout.tv_usec = 0;

  /* select returns 0 if timeout, 1 if input available, -1 if error. */
  return TEMP_FAILURE_RETRY (select (FD_SETSIZE,
                                     &set, NULL, NULL,
                                     &timeout));
}

//*


int main (int argc, char **argv) {
    int listenfd, connfd;
    struct sockaddr_in servaddr;
    pid_t childpid;

    char recvline[MAXLINE + 1];
    ssize_t n;
    
    int port;
    if (argc != 2) {
        port = 1883;
    }
    else{
        port = atoi(argv[1]);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(port);
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %d]\n",port);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

    /* ========================================================= */
    /* ========================================================= */
    /*                         EP1 INÍCIO                        */
    /* ========================================================= */
    /* ========================================================= */    
    char recvline2[MAXLINE + 1];
    ssize_t n2;
    int client_number = -1;
    int pipes_fd[MAXCLIENTS];
    char pipes[MAXCLIENTS][FDLENGTH];

    // Cria um arquivo de registro para armazenar os tópicos que cada cliente está inscrito.
    char register_pipe[REGISTERLENGTH] = {"/tmp/broker11795356register-XXXXXX"};
    mkstemp(register_pipe);  
    FILE * fp;    
    fp = fopen (register_pipe, "w");
    fprintf(fp,"%d/%d;",0,0);
    fclose(fp);
    /*
        Formato do arquivo de registro:
            Exemplo: 29/3;1,mac0352;0,daniel;0,fatemeh;
                29 = caracteres após o primeiro ';'
                3 = quantidade de tópicos/clientes existentes
                1,mac0352 = cliente 1 está inscrito no tópico mac0352
    */


	for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }
        client_number++;         
        // Cria fifo para cada cliente.
        char num[6];
        
        sprintf(num,"%d",client_number);
        memset(pipes[client_number],0,FDLENGTH);
        strcat(pipes[client_number],"/tmp/broker11795356-");
        strcat(pipes[client_number],num);
        strcat(pipes[client_number], "XXXXXX");
        mkstemp(pipes[client_number]);
        unlink((const char *) pipes[client_number]);
        if (mkfifo((const char *) pipes[client_number],0644) == -1) {
            perror("erro no mkfifo(\n");
        }
         

        if ( (childpid = fork()) == 0) {    
            /**** PROCESSO FILHO ****/

            printf("[Uma conexão aberta]\n");
            close(listenfd);            
            meu_cliente client;
            client.qtd_topics = 0;
            client.number = client_number;
            client.fd = -1;
            while (1){
                int checaConnfd;

                // Se cliente não for sub, sempre checa o connfd. Se cliente for sub, também é necessário checar o client.fd.
                if (client.fd != -1)
                    checaConnfd = input_timeout(connfd,COOLDOWN);                
                else
                    checaConnfd = 1;
                
                if (checaConnfd == 1){
                    n = read(connfd, recvline, MAXLINE);
                    if (n > 0){
                        recvline[n]=0;

                        int full_bytes[n+1];
                        for (int i = 0; i < n;i++)
                            full_bytes[i] = (int) (unsigned char) recvline[i];

                        client = interpret_message(connfd,full_bytes,n,client,pipes,pipes_fd,register_pipe);   
                        if (client.number == -1) break;
                    }
                }
                if (client.fd != -1){
                    if (input_timeout(client.fd,COOLDOWN) == 1){
                        n2=read(client.fd,recvline2,MAXLINE);
                        if (n2 > 0){
                            recvline2[n2]=0;
                            write(connfd,recvline2,n2);
                        }                    
                    }
                }
            }

            free(client.Id);
            if (client.fd != -1){
                close(client.fd);    
            }
            unlink((const char *) pipes[client_number]);
            
            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */  
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else{
            /**** PROCESSO PAI ****/
            close(connfd);
        }
    }
    exit(0);
}
