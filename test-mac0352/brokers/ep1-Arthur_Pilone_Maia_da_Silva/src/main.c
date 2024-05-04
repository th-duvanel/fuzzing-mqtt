/* Por Arthur Pilone Maia da Silva, NUSP 11795450
 * EP 1 - MAC0352
 * 
 *  Este arquivo consiste no programa principal do broker
 * desenvolvido neste EP e, como sugerido pelo professor no 
 * próprio enunciado, reaproveita o programa de um simples
 * servidor de echo - mac0352-servidor-exemplo-ep1.c .
 * A maioria dos comentários explicativos do código original 
 * foram apagados e as partes de minha autoria dentro deste
 * arquivo são marcadas e delimitadas por comentários.
 * 
 *  Para eventuais informações sobre a compilação, execução
 * e funcionamento deste broker, vide LEIAME ou a apresentação
 * em pdf.   
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
#include <time.h>

#include <signal.h>

/** Para usar o mkfifo() **/
#include <sys/stat.h>
/** Para usar o open e conseguir abrir o pipe **/
#include <fcntl.h>


/* ========================================================= */
/* ========================================================= */
/*                         EP1 INÍCIO                        */
/* ========================================================= */
/* ========================================================= */

#include "../include/utilities.h"
#include "../include/data.h"

#include "../include/connect.h"
#include "../include/publish.h"
#include "../include/subscribe.h"
#include "../include/unsubscribe.h"


#define MAXFIFOFILENAMELEN 40

connection_info con_stats;
item_index client_index;

/*
 *
 */
void not_alive_anymore(){
    if(con_stats.clean_session == 1){
       clear_client_info(client_index);
    }else{
        disconnect_client(client_index);
    }
    exit(0);
}

/* ========================================================= */
/* ========================================================= */
/*                      EP1 FIM (Pausa)                      */
/* ========================================================= */
/* ========================================================= */

int main (int argc, char **argv) {

    int listenfd, connfd;
    struct sockaddr_in servaddr;
    pid_t childpid;
    unsigned char recvline[MAXLINE + 1];
    ssize_t n;

    /*  Para evitar vazamento de memória no caso em que o processo filho seja 
     * morto antes que um cliente receba uma mensagem*, o char * que contém o 
     * endereço do seu fifo deve ser alocado em tempo de compilação.
     *  * Isso acontece pois a operação open(<fifo>,modo) "trava" a execução 
     * do programa até que algum outro processo abra o mesmo pipe <fifo> no 
     * modo escrita. Como esta operação precisa do caminho para o fifo, um
     * vetor de caracteres dinamicamente alocado só poderia ser liberado (free d)
     * após a abertura do arquivo que, como discutido, pode não acontecer. 
     */
    char fifo_file_name[MAXFIFOFILENAMELEN + 1];

    if (argc > 2) {
        fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
        fprintf(stderr,"Vai rodar um servidor de echo na porta <Porta> TCP\n");
        exit(1);
    }

    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket :(\n");
        exit(2);
    } 

    int port;

    if(argc == 1)       // Adição do EP 1
        port = 1883;       
    else
        port = atoi(argv[1]);
    
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

	for (;;) {

        if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
            perror("accept :(\n");
            exit(5);
        }

        if ( (childpid = fork()) == 0) {

            printf("[Uma conexão aberta]\n");

            close(listenfd);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 INÍCIO                        */
            /* ========================================================= */
            /* ========================================================= */

            unsigned short session_present = 0;

            read(connfd, recvline, MAXLINE);

            if(recvline[0] >> 4 != CONTROL_CONNECT){
                printf("[Cliente não iniciou conexão com pacote de controle CONNECT, fechando conexão]\n");
                exit(0);
            }

            short response = process_connect(recvline, &con_stats);

            if(response == -1){
                exit(0);
            }

            if(response != 0){
                unsigned char connack[4] = {(CONTROL_CONNACK << 4),2,0,response};
                write(connfd, connack , 4);
                exit(0);
            }
            if(con_stats.keep_alive_time > 0){
                alarm(con_stats.keep_alive_time * 1.5);
            }

            signal(SIGALRM,not_alive_anymore);

            client_index = connect_client(con_stats.client_id,con_stats.client_id_len,&session_present);
            unsigned char connack[4] = {32,2,(unsigned char) session_present,0};
            write(connfd, connack , 4);        

            if(fork() != 0){ // Um processo (filho) repassa o buffer para o socket
                char * fifo_path_to_be_freed = index_specific_path(PIPESDIR,client_index);
                strcpy(fifo_file_name, fifo_path_to_be_freed);
                
                free(fifo_path_to_be_freed);

                while (1) {
                    int my_pipe = open(fifo_file_name,O_RDONLY);

                    if(my_pipe == -1){
                        perror("fifo :(\n");
                        break;
                    }

                    if((n=read(my_pipe, recvline, MAXLINE)) <= 0)  
                        continue;

                    recvline[n]=0;
                    write(connfd, recvline, n);
                    close(my_pipe);
                }

                
            }else{           // Processo pai lê entrada do socket
                while((n=read(connfd, recvline, MAXLINE)) > 0){
                    recvline[n] = 0;

                    unsigned char control_code = recvline[0] >> 4;
                    short exiting = 0;

                    switch(control_code){

                        case CONTROL_PUBLISH:
                        
                            if(con_stats.keep_alive_time > 0){
                                alarm(con_stats.keep_alive_time * 1.5);
                            }

                            exiting = process_publish(recvline);

                            break;

                        case CONTROL_SUBSCRIBE:

                            if(con_stats.keep_alive_time > 0){
                                alarm(con_stats.keep_alive_time * 1.5);
                            }

                            exiting = process_subscribe(recvline, connfd, client_index);

                            break;
                        
                        case CONTROL_UNSUBSCRIBE: 

                            if(con_stats.keep_alive_time > 0){
                                alarm(con_stats.keep_alive_time * 1.5);
                            }

                            exiting = process_unsubscribe(recvline,connfd,client_index);

                            break;

                        case CONTROL_PINGREQ:
                            
                            if(con_stats.keep_alive_time > 0){
                                alarm(con_stats.keep_alive_time * 1.5);
                            }

                            if( (recvline[0] & 15) != 0){    //Checar bits reservados
                                exiting = 1;
                                break;
                            }

                            unsigned char pingresp = CONTROL_PINGRESP << 4;
                            unsigned char response[2] = {pingresp,0};

                            write(connfd,response,2);

                            break;

                        default:
                            exiting = 1;
                            break;
                    }

                    if(exiting)
                        break;
                    
                }
            }
             
            printf("[Uma conexão fechada]\n");
            if(con_stats.clean_session == 1){
                clear_client_info(client_index);
            }else{
                disconnect_client(client_index);
            }
            exit(0);

            /* ========================================================= */
            /* ========================================================= */
            /*                         EP1 FIM                           */
            /* ========================================================= */
            /* ========================================================= */
        }
        else
            close(connfd);
    }
    exit(0);
}