#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>

#include <stdbool.h>
#include <string.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <unistd.h>
#include <netinet/in.h>

#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <stdint.h>

#include <signal.h>

/* socket[] -> bind[] -> listen[] -> accept[] */
int main(int argc, char **argv) {

    /* Estrutura de dados - lista ligada

        Node_File_Desc são dos nós de files descriptors para pipe.
        Node_Topic são dos nós de dos tópicos e contém o início das 
        listas de pipes.
    */
    typedef struct Node_File_Desc node_fd;
    struct Node_File_Desc {
        int pipe_fd[2];
        node_fd *next_fd;  
    };

    typedef struct Node_Topic node_topic;
    struct Node_Topic {
        char *client_topic;
        node_fd *chain_fd;
        node_topic *next_topic;
    };

    // socket[]
    int sockfd_server;
    if((sockfd_server = socket(AF_INET, SOCK_STREAM, 0)) == -1) 
    {
        printf("\nError: socket failed!\n\n");
        return 1;
    }

    struct sockaddr_in sock_server_addr;
    sock_server_addr.sin_family = AF_INET;
    if (argc == 1) {
        sock_server_addr.sin_port = htons(1883);
        printf("\nInicializando servidor Broker EP1 na porta 1883\n\n");        
    }
    else 
    {
        sock_server_addr.sin_port = htons(atoi(argv[1]));
        printf("\nInicializando servidor Broker EP1 na porta %d\n\n", atoi(argv[1]));        
    }
    sock_server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    
    // bind[]
    if((bind(sockfd_server,
        (const struct sockaddr *) &sock_server_addr,
        sizeof(sock_server_addr))) == -1) 
    {
        printf("\nError: bind failed!\n\n");
        return 2;
    }

    // listen[]
    if((listen(sockfd_server, 10)) == -1) 
    {
        printf("\nError: listen failed!\n\n");
        return 3;
    }

    node_topic *head = (node_topic *) malloc(sizeof(node_topic));
    head->next_topic = NULL;
    head->chain_fd = NULL;

    // Laço de execução sempre aceitando conexões e gerando processos
    // para elas.
    int count = 0;
    while (1) {

        // accept[]
        int sockfd_client;
        if ((sockfd_client = accept(sockfd_server,
                                    (struct sockaddr *) NULL,
                                    NULL)) == -1 ) {
            printf("\nError: accept failed!\n\n");
        } 
        else 
        {
            count++;
            printf("Connection number %d established:\n", count);

            unsigned char *ch = malloc(128); 
            if(recv(sockfd_client,  ch, 128 ,0) == -1) 
            {
                printf("\nError: recv failed!\n\n");
                return 5;
            }
            free(ch);


            /*CONNACK_____________________________________________________*/

            typedef struct connack_header a_header;
            struct connack_header {
                uint8_t fixed_header_flags;
                uint8_t remaining_length;
                uint8_t connect_acknowledge_flags; 
                uint8_t return_code;   
            };

            a_header ah;
                /*Fixed_Header*/
            ah.fixed_header_flags = 32;
            ah.remaining_length = 2;
                /*Variable_Header*/
            ah.connect_acknowledge_flags = 0;
            ah.return_code = 0;

            if(send(sockfd_client, &ah, sizeof(ah), 0) == -1) 
            {
                printf("\nError: send failed!\n\n");
                return 6;
            }    
        

            /*SUB/PUB_____________________________________________________*/

            unsigned char * 
            recv_line = (unsigned char *) malloc(512);
            bzero((void *)recv_line, 512);
            if(recv(sockfd_client,  recv_line, 512, 0) == -1) 
            {
                printf("\nError: recv failed!\n\n");
                return 5;
            }

            uint8_t sub_or_pub = recv_line[0];
            sub_or_pub = (sub_or_pub >> 4);
            uint8_t tl = 0;
            char  *client_topic;

            //Prepara info para a situação de publicador ou inscrito.
            if (sub_or_pub == 8)
            {
                tl = recv_line[4];
                tl = (tl << 8) + recv_line[5];
                client_topic = malloc(sizeof(char)*tl + 1);
                for (int i = 6; i < 6 + tl; i++)
                {
                    client_topic[i - 6] = recv_line[i];
                }
                client_topic[tl] = '\0';
            } 
            else if (sub_or_pub == 3) 
            {
                tl = recv_line[2];
                tl = (tl << 8) + recv_line[3];
                //ml = recv_line[1] - tl - 2;
                client_topic = malloc(sizeof(char)*tl + 1);
                for (int i = 4; i < 4 + tl; i++)
                {
                    client_topic[i - 4] = recv_line[i];
                }
                client_topic[tl] = '\0';
            }
            else 
            {
                printf("\nError: client is no subscriver nor publisher!\n\n");
                return 8;
            }

            // Preenche e verifica estrutura de dados referentes a tópicos e pipes.
            node_topic *p;
            node_topic *q = NULL;
            node_fd *fd;
            bool found = false;
            for (p = head->next_topic; found != true && p != NULL; p = p->next_topic)
            {
                if (strncmp (p->client_topic, client_topic, tl) == 0)
                {
                    q = p;
                    found = true;
                }
                
            }
            if (!found)
            {
                node_topic * new_sub = (node_topic *) malloc(sizeof(node_topic));
                new_sub->client_topic = (char *) malloc(sizeof(tl + 1));
                strncpy(new_sub->client_topic, client_topic, tl + 1);
                new_sub->client_topic[tl] = '\0';
                new_sub->chain_fd = NULL;
                new_sub->next_topic = head->next_topic;
                head->next_topic = new_sub;
                q = new_sub;
            }
            if (sub_or_pub == 8)
            {
                fd = (node_fd *) malloc(sizeof(node_fd));
                pipe(fd->pipe_fd);
                fd->next_fd = q->chain_fd;
                q->chain_fd = fd;
            } 
            else if (sub_or_pub == 3)
            {
                fd = q->chain_fd;
            }
        
            // Cria processo filho para lidar com cliente
            int parent = fork();
            if (parent == -1)
            {
                printf("\nError: fork failed!\n\n");
                return 8;
            }
            if (!parent) 
            {
                close(sockfd_server);
                printf("    Process %d taking care of client:\n", getpid());

                if (sub_or_pub == 8) 
                {
                    /*SUBACK______________________________________________________*/

                    typedef struct suback_header sa_header;
                    struct suback_header {
                        uint8_t fixed_header_flags;
                        uint8_t remaining_length;
                        uint16_t message_identifier;
                        uint8_t return_code;
                    };

                    sa_header sa;
                        /*Fixed_Header*/
                    sa.fixed_header_flags = 9 << 4;
                    sa.remaining_length = 3;
                        /*Variable_Header*/
                    sa.message_identifier = htons(1);
                        /*Payload*/
                    sa.return_code = 0;
                    if(send(sockfd_client, &sa, sizeof(sa), 0) == -1) 
                    {
                        printf("\nError: send failed!\n\n");
                        return 6;
                    }   

                    printf("    Client is a Subscriber of topic: %s\n\n", client_topic);
                    
                    // Limpa estrutura de dados desnecessária para incrito.
                    node_topic *free_topic= NULL;
                    for (p = head->next_topic;  p != NULL; p = p->next_topic)
                    {
                        node_fd *free_node = NULL;
                        for (node_fd *f = p->chain_fd; f != NULL; f = f->next_fd)
                        {
                            if (f->pipe_fd != fd->pipe_fd)
                            {
                                close(f->pipe_fd[0]);
                                close(f->pipe_fd[1]);
                            }
                            if((free_node != fd) && (free_node != NULL)) free(free_node);
                            free_node = f;
                        }
                        if(free_topic != NULL) {
                            free(free_topic->client_topic);
                            free(free_topic);
                        }
                        free_topic = p;
                    }
                    free(head);
                    free(client_topic);
                    free(recv_line);

                    //Cria terceiro processo filho para escutar por pedidos de desconexão.
                    int pid_aux;
                    if ((pid_aux = fork()) == -1)
                    {
                        printf("\nError: fork failed!\n\n");
                        return 8;
                    } 
                    else if (pid_aux == 0)
                    {
                        while(1) 
                        {
                            int size = 0;
                            if(read(fd->pipe_fd[0], &size, sizeof(int)) == -1) 
                            {
                                printf("\nError: read failed!\n\n");
                                return 8;
                            } 

                            unsigned char * 
                            send_line = (unsigned char *) malloc(size);
                            bzero((void *)send_line, size);
                            if(read(fd->pipe_fd[0], send_line, size) == -1) 
                            {
                                printf("\nError: read failed!\n\n");
                                return 8;
                            } 
                        
                            if(send(sockfd_client, send_line, size, 0) == -1) 
                            {
                                printf("\nError: send failed!\n\n");
                                return 6;
                            }   
                            printf("    ...sended to subscriber\n");
                            free(send_line);
                        } 
                    }
                    else 
                    {
                        typedef struct disreq_header dr_header;
                        struct disreq_header {
                            uint8_t fixed_header_flags;
                            uint8_t remaining_length;
                        };

                        dr_header dr;
                        int cl = 1;
                        while(cl) 
                        {
                            if(recv(sockfd_client, &dr, sizeof(dr), 0) == -1) 
                            {
                                printf("\nError: recv failed!\n\n");
                                kill(pid_aux, SIGKILL);
                                return 5;
                            }
                            if((dr.fixed_header_flags >> 4) == 14) cl = 0;
                        }
                        kill(pid_aux, SIGKILL);
                    }

                    printf("Process %d closing subscriber connection:\n\n", getpid());
                    close(sockfd_client);
                    return 0; 
                    
                } else if (sub_or_pub == 3) {

                    printf("    Client is a Publisher of topic: %s\n\n", client_topic);

                    int size = 2 + recv_line[1];
                    unsigned char * 
                    send_line = (unsigned char *) malloc(size);
                    for (int i = 0; i < size; i++)
                    {
                        send_line[i] = recv_line[i];
                    }               
                    free(recv_line);

                    // Faz envio para os pipes referentes aos processos dos inscritos.
                    for (node_fd *f = fd; f != NULL; f = f->next_fd)
                    {
                        if(write(f->pipe_fd[1], &size, sizeof(int)) == -1) 
                        {
                            printf("\nError: write failed!\n\n");
                            return 7;
                        } 
                        if(write(f->pipe_fd[1], send_line, size) == -1) 
                        {
                            printf("\nError: write failed!\n\n");
                            return 7;
                        } 
                    }
                    free(send_line);

                    node_topic *free_topic= NULL;
                    for (p = head->next_topic;  p != NULL; p = p->next_topic)
                    {
                        node_fd *free_node = NULL;
                        for (node_fd *f = p->chain_fd; f != NULL; f = f->next_fd)
                        {
                            close(f->pipe_fd[0]);
                            close(f->pipe_fd[1]);
                            if(free_node != NULL) free(free_node);
                            free_node = f;
                        }
                        if(free_topic != NULL) free(free_topic);
                        free_topic = p;
                    }

                    /*DISACK______________________________________________________*/

                    typedef struct disack_header da_header;
                    struct disack_header {
                        uint8_t fixed_header_flags;
                        uint8_t remaining_length;
                    };

                    da_header da;
                    if(recv(sockfd_client,  &da, sizeof(da_header),0) == -1) 
                    {
                        printf("\nError: recv failed!\n\n");
                        return 5;
                    }            

                    printf("Process %d closing publisher connection:\n\n", getpid());

                    free(client_topic);
                    free(head);
                    close(sockfd_client);
                    
                    return 0;
                } else {
                    printf("\nError:  client is no subscriber nor publisher\n\n");
                    return 8;
                }
                return 0;
            }
            free(recv_line);
        
        }  
    }
     return 0;
}

