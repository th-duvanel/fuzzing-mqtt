/*
* NOME: GUSTAVO SANTOS MORAIS
* NUSP: 11221932
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
#include <dirent.h>
#include <pthread.h>

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096


typedef struct topic{
    int fd;
    char topicName[MAXLINE+1];
    int topicLenght;
    int socket;
} Topic;


void * Thread(void *t){
    Topic *topic = (Topic *)t;
    char topicMsg[MAXLINE+1];
    ssize_t n;


    while((n=read(topic->fd, topicMsg, MAXLINE)) > 0){
        topicMsg[MAXLINE] = 0;

        char resp[4+topic->topicLenght+n];
        
        resp[0] = 0x30;
        resp[1] = 2+topic->topicLenght+n;
        resp[2] = 0x00;
        resp[3] = topic->topicLenght;

        int i;
        for(i = 0; i < topic->topicLenght; i++){
            resp[4+i] = topic->topicName[i];
        }
        for(int j = 0; j < n; j++){
            resp[i+4+j] = topicMsg[j];
        }

        write(topic->socket, resp, 4+topic->topicLenght+n);
    }
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
    /** Variável que vai contar quantos clientes estão conectados.*/
    int cliente;
    cliente = 0;

 
    if (argc != 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }


    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    }

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(atoi(argv[1]));
    if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
        perror("bind :(\n");
        exit(3);
    }

    if (listen(listenfd, LISTENQ) == -1) {
        perror("listen :(\n");
        exit(4);
    }

    printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
    printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");
   

	for (;;) {
        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }

        cliente++;
      

        if ( (childpid = fork()) == 0) {
            /**** PROCESSO FILHO ****/
            printf("[Uma conexão aberta]\n");
            /* Já que está no processo filho, não precisa mais do socket
             * listenfd. Só o processo pai precisa deste socket. */
            close(listenfd);
         
            int ehSub = 0;
            char subPaths[100][MAXLINE];
            memset(subPaths, 0x00, 100);
            int subFD[100];
            memset(subFD, -1, 100);
            pthread_t tid[100];
            memset(tid, -1, 100);
            while ((n=read(connfd, recvline, MAXLINE)) > 0) {
                recvline[n]=0;

                uint8_t mask = 0xf0;
                int packetType = mask & recvline[0];
                packetType = packetType >> 4;

                /*Reconhece o CONNECT e responde com CONNACK*/
                if(packetType == 1){
                    unsigned char respConnack[4] = {0x20,2,0x00,0x00};
                    write(connfd, respConnack, 4);
                }
                /*SUBSCRIBE*/
                else if(packetType == 8){
                    ehSub++; /*ativar a flag para avisar que este processo é de um sub*/

                    int i = 5; /*byte referente ao tamanho do primeiro tópico*/


                    int topicCount = 0;

                    while(i < n-1){           
                        int topicLenght = recvline[i];    
                        char topicName[topicLenght+1];         
                        strncpy(topicName, &recvline[i+1], topicLenght);
                        topicName[topicLenght] = 0;

                        /*crio a pasta daquele tópico*/
                        char path[MAXLINE] = "/tmp/";
                        strcat(path, topicName);
                        
                        if(mkdir(path, 0777) == -1){
                            perror("mkdir :(\n");
                        }   

                        char file[MAXLINE];
                        strcpy(file, path);
                        char clientID[10];
                        sprintf(clientID, "%d",cliente);
                        strcat(file, "/");
                        strcat(file, clientID);
                        
                        if(mkfifo(file, 0644) == -1){
                            perror("mkfifo :(\n");
                        }

                        Topic *t = malloc(sizeof(Topic));

                        t->fd = open(file, O_RDWR);
                        strcpy(subPaths[topicCount], file);

                        subFD[topicCount] = t->fd;
                        strcpy(t->topicName, topicName);
                        t->topicLenght = topicLenght;
                        t->socket = connfd;
                        
                        pthread_create(&tid[topicCount],NULL,Thread,(void *)t);

                        i = i + topicLenght + 3; /*pulando os bytes do QoS e lenght MSB do próximo tópico*/
                        topicCount++;
                    }

                    unsigned char resp[4+topicCount];
                    resp[0] = 0x90;
                    resp[1] = 2+topicCount;

                    /*posições da msg do SUBSCRIBE que contém o id*/
                    resp[2] = recvline[2];
                    resp[3] = recvline[3]; 

                    for(int k = 4; k < topicCount+4; k++){
                        resp[k] = 0; /*colocando QoS como 0 para todos os tópicos*/
                    }

                    /*respondendo com SUBACK*/
                    write(connfd, resp, topicCount+4);
                }
                /*PUBLISH*/
                else if(packetType == 3){
                    int topicLenght = recvline[3];
                    unsigned char topicName[topicLenght+1];
                    strncpy(topicName, &recvline[4], topicLenght);
                    topicName[topicLenght] = 0;

                    int remainingLenght = recvline[1];
                    int variableHeaderLenght = topicLenght + 2;
                    int msgLenght = remainingLenght - variableHeaderLenght;

                    unsigned char msg[msgLenght+1];
                    strncpy(msg, &recvline[4+topicLenght], msgLenght);
                    msg[msgLenght] = 0;

                    char path[MAXLINE] = "/tmp/";
                    strcat(path, topicName);
                    DIR *topicDir; 
                    if((topicDir = opendir(path)) == NULL){
                        continue;
                    }

                    struct dirent *arq;
                    while((arq = readdir(topicDir)) != NULL){
                        if(strcmp(arq->d_name,".") != 0 && strcmp(arq->d_name,"..") != 0){
                            char arqName[MAXLINE];
                            strcpy(arqName, path);
                            strcat(arqName, "/");
                            strcat(arqName, arq->d_name);
                            int subArq = open(arqName, O_RDWR);
                            write(subArq, msg, msgLenght+1);
                            close(subArq);
                        }
                    }

                    closedir(topicDir);
                }
                /*PINGREQ*/
                else if(packetType == 12){
                    /*Respondendo com PINGRESP*/
                    unsigned char resp[2] = {0xd0,0x00};
                    write(connfd, resp, 2);
                }
                /*DISCONNECT*/
                else if(packetType == 14){

                    /*Cancelar a execucão das threads referentes ao processo do sub*/
                    if(ehSub){
                        for(int i = 0; tid[i] != -1; i++){
                            pthread_cancel(tid[i]);
                            pthread_join(tid[i], NULL);
                            close(subFD[i]);
                            unlink(subPaths[i]);
                        }
                    }
                }
            }
            /* Após ter feito toda a troca de informação com o cliente,
             * pode finalizar o processo filho */
            printf("[Uma conexão fechada]\n");
            exit(0);
        }
        else
            /**** PROCESSO PAI ****/
            close(connfd);
    }
    exit(0);
}
