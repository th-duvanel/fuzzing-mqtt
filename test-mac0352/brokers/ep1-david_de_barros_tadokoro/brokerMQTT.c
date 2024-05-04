/* ========================================================= */
/*                        EP1 - MAC0352                      */
/* ========================================================= */
/*   NOME: David de Barros Tadokoro                          */
/*   NUSP: 10300507                                          */
/*   DATA: 25/04/2022                                        */
/*   PROF: Daniel Macedo Batista                             */
/* ========================================================= */
/* ========================================================= */

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
/** p/ usar o mkfifo() **/
#include <sys/stat.h>
/** p/ usar o open e conseguir abrir o pipe **/
#include <fcntl.h>
/** p/ usar opendir(), readdir() e closedir() **/
#include<dirent.h>

#define LISTENQ 1000
#define MAXDATASIZE 100
#define MAXLINE 4096

/* Esta função lê dois bytes que estão codificados usando o esquema
 * de codificação do 'Remaining Length' descrito na RFC do MQTT 3.1.1
 * e decodifica-os p/ um inteiro sem sinal. */
unsigned int decodificaCompRest (int connfd) {
  unsigned char byte_codificado;
  unsigned int mult = 1;
  unsigned int valor = 0;
  do {
    read(connfd, &byte_codificado, sizeof(byte_codificado));
    valor += (byte_codificado & 127) * mult;
    mult = mult * 128;
  } while((byte_codificado & 128) != 0);
  return valor;
}

/* Esta função codifica um inteiro sem sinal no esquema de codificação
 * do 'Remaining Length' descrito na RFC do MQTT 3.1.1. Esta função não
 * é muito versátil, pois também envia os bytes que codificam o inteiro
 * na conexão. */
void codificaCompRest (int connfd, unsigned int comp_rest) {
  unsigned char byte_codificado;
  do {
    byte_codificado = comp_rest % 128;
    comp_rest = comp_rest / 128;
    if (comp_rest > 0) {
      byte_codificado = byte_codificado | 128;
    }
    write(connfd, &byte_codificado, 1);
  } while(comp_rest > 0);
}

/* Esta função lê a mensagem publicada do pacote PUBLISH e a escreve
 * nos pipes referentes aos incritos do tópico em questão. A forma como
 * isto é feito é iterando por cada pipe do diretório que corresponde ao
 * tópico. */
void leMensagemEnvia (int connfd, unsigned int comp_rest, char* path_top) {
  int pipefd;
  char* mensagem;
  char path_pipe[MAXLINE + 1];
  DIR *dir;
  struct dirent *file;

  mensagem = malloc(comp_rest);
  read(connfd, mensagem, comp_rest);

  dir = opendir(path_top);
  if (dir) {
    while ((file = readdir(dir)) != NULL) {
      snprintf(path_pipe, sizeof(path_pipe), "%s/%s", path_top, file->d_name);
      pipefd = open(path_pipe, O_WRONLY);
      write(pipefd, mensagem, comp_rest);
      close(pipefd);
    }
    closedir(dir);
  }
}

/* Função que fecha a comunicação com um cliente e encerra o processo filho
 * que cuida dele. */
void desconecta(int connfd) {
  close(connfd);
  exit(0);
}

int main (int argc, char **argv) {
  /* Descritores dos sockets e do pipe */
  int listenfd, connfd, pipefd;
  /* Struct que guarda informações sobre o socket */
  struct sockaddr_in servaddr;
  /* Buffers p/ armazenar caracteres */
  char recvline[MAXLINE + 1];
  char sendline[MAXLINE + 1];
  char topico[MAXLINE + 1];    // p/ nome do tópico
  char path_top[MAXLINE + 1];  // p/ path do tópico (diretório) onde ficam os pipes
  char path_pipe[MAXLINE + 1]; // p/ path do pipe
  /* Armazena um byte p/ manipulações byte a byte */
  unsigned char byte;
  /* Lista de dois bytes usado p/ armazenar id da mensagem (SUBSCRIBE e SUBACK) */
  unsigned char msg_id[2];
  /* Inteiros que guardam os campos 'Remaining Length' e 'Topic Length' */
  unsigned int comp_rest, comp_topico;
  /* Booleanos p/ indicar estado dos processos */
  int conectado = 0, subscriber = 0;
  /* Variáveis auxiliares */
  int i;
  ssize_t n;
  pid_t childpid;

  /* Tratando erro de execução */
  if (argc != 2) {
    fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
    fprintf(stderr,"Vai rodar um broker MQTT na porta <Porta> TCP\n");
    exit(1);
  }

  /* Criando o socket especial de 'listen' */
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
    perror("socket :(\n");
    exit(2);
  }

  /* Limpando o struct servaddr */
  bzero(&servaddr, sizeof(servaddr));
  servaddr.sin_family      = AF_INET;
  servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servaddr.sin_port        = htons(atoi(argv[1]));
  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == -1) {
    perror("bind :(\n");
    exit(3);
  }

  /* Abrindo socket especial 'listen' */
  if (listen(listenfd, LISTENQ) == -1) {
    perror("listen :(\n");
    exit(4);
  }

  /* Neste momento o servidor já está pronto p/ aceitar conexões de clientes */
  printf("[Servidor no ar. Aguardando conexões na porta %s]\n",argv[1]);
  printf("[Para finalizar, pressione CTRL+c ou rode um kill ou killall]\n");

  /* Loop principal do servidor p/ aceitar conexões */
	for (;;) {
    /* Aceitando conexão de um cliente */
    if ((connfd = accept(listenfd, (struct sockaddr *) NULL, NULL)) == -1 ) {
      perror("accept :(\n");
      exit(5);
    }

    /* Criando um processo novo p/ cuidar de cada cliente */
    if ((childpid = fork()) == 0) {
      printf("[Conexão aberta no processo filho %d]\n", getpid());
      close(listenfd);

      /* Loop p/ receber 'MQTT Control Packets' deste cliente */
      for (;;) {
        /* Lendo 'MQTT Control Packet Type' e extraindo seu código */
        read(connfd, &byte, 1);
        byte = byte >> 4;
        /* Decodificando os próximos 2 bytes que representam o 'Remaining Length' */
        comp_rest = decodificaCompRest(connfd);

        switch (byte) {
          /***********/
          /* CONNECT */
          /***********/
          case 1: {
            /* Conferindo se o cliente está usando a versão 3.1.1 do MQTT */
            read(connfd, recvline, 7);
            const unsigned char aux[] = "\x00\x04\x4d\x51\x54\x54\x04";
            for (i = 0; i != 8; i++) {
              if (recvline[i] != aux[i]) {
                printf("['Protocol Name' ou 'Protocol Level' não suportado!]\nFechando a conexão no processo filho %d]\n", getpid());
                desconecta(connfd);
              }
            }

            /* Lendo bytes restantes que não foram considerados (veja slides p/ explicação) */
            read(connfd, recvline, comp_rest - 7);

            /* Permitindo que este cliente envie os pacotes 'PUBLISH' e 'SUBSCRIBE' */
            conectado = 1;

            /***********/
            /* CONNACK */
            /***********/
            /* Resposta padrão p/ conexão estabelecida com sucesso */
            snprintf(sendline, sizeof(sendline), "%c%c%c%c", '\x20', '\x02', '\x00', '\x00');
            write(connfd, sendline, 4);

            break;
          }

          /***********/
          /* PUBLISH */
          /***********/
          case 3: {
            if (!conectado) desconecta(connfd);

            /* Lendo e manipulando os bytes referentes ao 'Topic Length' */
            read(connfd, recvline, 2);
            comp_topico = recvline[1] | recvline[0] << 8;

            /* Lendo o tópico */
            n = read(connfd, recvline, comp_topico);
            recvline[n] = 0;
            comp_rest = comp_rest - 2 - comp_topico; // atualizando o 'Remaining Length' */

            /* Preparando o path do diretório referente ao tópico, lendo a mensagem
             * publicada e enviando (escrevendo) p/ todos os clientes (pipes) inscritos */
            snprintf(path_top, sizeof(path_top), "%s/%s", "/tmp", recvline);
            leMensagemEnvia(connfd, comp_rest, path_top);

            break;
          }

          /*************/
          /* SUBSCRIBE */
          /*************/
          case 8: {
            if (!conectado) desconecta(connfd);

            /* Lendo o 'Message Identifier' */
            read(connfd, msg_id, 2);
            /* Lendo e manipulando os bytes referentes ao 'Topic Length' */
            read(connfd, recvline, 2);
            comp_topico = recvline[1] | recvline[0] << 8;
            /* Lendo o 'Topic Filter' */
            n = read(connfd, topico, comp_topico);
            topico[n] = 0;

            /* Preparando o path do tópico e criando o diretório referente */
            snprintf(path_top, sizeof(path_top), "%s/%s/", "/tmp", topico);
            mkdir(path_top, 0755);
            /* Preparando o template do path do pipe */
            snprintf(path_pipe, sizeof(path_pipe), "%s%s", path_top, "pipe.XXXXXX");
            /* Criando um arquivo único e deletando-o (explicação nos slides) */
            mkstemp(path_pipe);
            remove(path_pipe);
            /* Criando pipe */
            mkfifo((const char *) path_pipe, 0644);

            /* Lendo byte referente ao QoS da inscrição (ignorado) */
            read(connfd, &byte, 1);

            /* Identificando este cliente como subscriber */
            subscriber = 1;

            /* Processo filho criado fica esperando publisher escrever no pipe */
            if (fork() == 0) {
              for(;;) {
                /* Abrindo pipe, lendo a mensagem e fechando pipe */
                pipefd = open(path_pipe, O_RDONLY);
                n = read(pipefd, recvline, MAXLINE);
                recvline[n] = 0;
                close(pipefd);
                /* Calculando o 'Remaining Length' do pacote a ser enviado */
                comp_rest = 2 + comp_topico + strlen(recvline);
                /* Preparando o pacote e enviando-o ao subscriber */
                write(connfd, "\x30", 1);
                codificaCompRest(connfd, comp_rest);
                byte = comp_topico >> 8;
                write(connfd, &byte, 1);
                byte = comp_topico & 0xFF;
                write(connfd, &byte, 1);
                write(connfd, topico, strlen(topico));
                write(connfd, recvline, strlen(recvline));
              }
            }

            /**********/
            /* SUBACK */
            /**********/
            /* Resposta padrão p/ inscrição bem sucedida */
            snprintf(sendline, sizeof(sendline), "%c%c%c%c%c", '\x90', '\x03', msg_id[0], msg_id[1], '\x00');
            write(connfd, sendline, 5);

            break;
          }

          /**************/
          /* DISCONNECT */
          /**************/
          case 14: {
            /* Removendo pipe de subscriber (se for um) */
            if (subscriber) remove(path_pipe);

            printf("[Conexão fechada no processo filho %d]\n", getpid());
            desconecta(connfd);
            break;
          }

          /* Caso seja enviado um pacote não implementado, fecha conexão */
          default: {
            /* Removendo pipe de subscriber (se for um) */
            if (subscriber) remove(path_pipe);

            printf("['MQTT Control Packet' não implementado]\n[Fechando a conexão no processo filho %d]\n", getpid());
            desconecta(connfd);
          }
        }
      }
    }

    /* Processo pai do servidor apenas fecha a conexão (não vai precisar) e volta
     * no loop p/ aceitar novas conexões */
    else close(connfd);
  }
  exit(0);
}
