#include "functions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <fcntl.h>
#include <time.h>
#include <signal.h>


int directoryExists(char * path) {
    struct stat st;
    stat(path, &st);
    if (S_ISDIR(st.st_mode))
        return 1;
    return 0;
}


void createDirectory(char * path) {
    /* Não executa se o diretório já existir */
    if(!directoryExists(path)) { 
        if(mkdir(path, 0755) != 0){
            //printf("mkdir :(");
            exit(1);
        }
    }
}


void createRootDirectory() {
    createDirectory(ROOT_PATH);
}


void cleanRootDirectory() {
    /* Não executa se o diretório NÃO existir */
    if(directoryExists(ROOT_PATH) && rmdir(ROOT_PATH) == -1) { 
        printf("%s já existe e não está vazio.\nApague-o para bom funcionamento do programa.\n", ROOT_PATH);
        exit(1);
    };
}


void randomAlphanumericString(char * string, unsigned int length) {
    unsigned int i;
    static const char alphanumeric[] = 
        "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    
    srand( (unsigned) time(NULL) * getpid());
    for(i = 0; i < length; i++) {
        string[i] = alphanumeric[rand() % strlen(alphanumeric)];
    }
    string[i] = 0;
}


void expandPath(char * path, char * source, char * child) {
    path[0] = 0;
    strcpy(path, source);
    strcat(path, "/");
    strcat(path, child);
}


unsigned int copyRemainingLength(int connfd, unsigned char * recvline) {
    unsigned char rbyte; /* Armazena o byte do Remaining Length sendo lido no momento */
    short int rstart;
    short int i;

    read(connfd, &rbyte, 1);

    i = 1;
    if (rbyte & 0x80) {
        for(i = 1; i < 4 && (rbyte & 0x80); i++){
            recvline[i] = rbyte;
            read(connfd, &rbyte, 1);
        }
    }
    recvline[i] = rbyte;

    rstart = i + 1; 

    return rstart; /* Devolve a localização do primeiro byte após o Fixed Header */
}


unsigned int getRemainingLength(unsigned char * recvline) {
    unsigned int rlength; /* O comprimento do restante do pacote depois do Fixed Header */
    short int i;

    rlength = 0;
    i = 1;
    
    if (recvline[1] & 0x80) {
        for(i = 1; i < 4 && (recvline[i] & 0x80); i++)
            rlength += (recvline[i] & 0x7F) << (7 * (i - 1));
    }
    rlength += (recvline[i] & 0x7F) << (7 * (i - 1));

    return rlength;
}


unsigned short int getTopicStart(unsigned char * recvline, short int hasPacketIdentifier) {
    unsigned short int i;
    i = 2;
    
    while(recvline[i-1] & 0x80)
        i++;

    if (hasPacketIdentifier) i += 2;

    return i; /* Devolve a localização do primeiro byte que indica o comprimento do tópico */
}


unsigned short int getPacketIdentifierStart(unsigned char * recvline) {
    unsigned short int i;
    i = getTopicStart(recvline, 1);
    return i - 2; /* A localização do primeiro byte do Packet Identifier */
}


unsigned int getTopicLength(unsigned char * recvline, short int hasPacketIdentifier) {
    short int i;
    i = getTopicStart(recvline, hasPacketIdentifier);
    return (recvline[i] << 8) + recvline[i + 1]; /* Devolve o comprimento do tópico */
}


void getTopicName(unsigned char * recvline, char * tname, short int hasPacketIdentifier) {
    unsigned int tstart, j, tlength;

    tstart = getTopicStart(recvline, hasPacketIdentifier) + 2; 
    /* Soma 2 para pular os bytes que indicam o tamanho do tópico */
    
    /* Quantidade de caracteres que o tópico possui */
    tlength = getTopicLength(recvline, hasPacketIdentifier); 

    for(j = 0; j < tlength; j++)
        tname[j] = recvline[tstart + j];
    tname[j] = 0;
}

void writeConnack(int connfd) {
    static unsigned char connack[] = {0x20, 0x02, 0x00, 0x00};
    write(connfd, connack, 4);
}


void writePingresp(int connfd) {
    static unsigned char pingresp[] = {0xd0, 0x00};
    write(connfd, pingresp, 2);
}


void writeSuback(unsigned char * recvline, int connfd, short int hasFailed) {
    static unsigned char suback[5] = {0x90, 0x03, 0x00, 0x01, 0x00};
    static short int pistart; /* Posição do primeiro byte do Packet Identifier */

    /* Adiciona o Packet Identifier ao SUBACK */
    pistart = getPacketIdentifierStart(recvline);
    suback[2] = recvline[pistart];
    suback[3] = recvline[pistart + 1];

    /* Adiciona o último byte de sucesso (para QoS=0) ou de falha */
    if (hasFailed)
        suback[4] = 0x80;
    else
        suback[4] = 0x00;

    write(connfd, suback, 5);
}



void writeOnPipes(unsigned char * recvline, ssize_t n, char * tpath) {
    static char fpath[ROOT_PATH_LENGTH + MAX_TOPIC_LENGTH + 12]; /* 12 = 2 (/) + 10 (comprimento do nome do pipe) */
    struct dirent *dp;
    int fd, i;

    DIR * dir = opendir(tpath);
    i = 0;

    /* Escreve recvline em todos os arquivos dentro do diretório */
    while ((dp = readdir(dir)) != NULL) {
        if (i < 2) { i++; continue; } /* Pula os itens . e .. */

        expandPath(fpath, tpath, dp->d_name); /* Armazena o path para o pipe em fpath */

        fd = open(fpath, O_WRONLY);
        write(fd, recvline, n);
        close(fd);
    }

    closedir(dir);
}


void publish(unsigned char * recvline, ssize_t n) {
    static char tname[MAX_TOPIC_LENGTH + 1]; /* Nome do tópico */
    static char tpath[ROOT_PATH_LENGTH + MAX_TOPIC_LENGTH + 2]; /* Path para a pasta do tópico */

    getTopicName(recvline, tname, 0); /* 0 pois PUBLISH com QoS=0 não tem Packet Identifier */
    expandPath(tpath, ROOT_PATH, tname); /* Gera o path para o diretório do tópico */
    createDirectory(tpath); /* Cria um diretório para o tópico caso ainda não exista */

    /* Escreve o pacote em todos os pipes registrados neste tópico */
    writeOnPipes(recvline, n, tpath);
}


void readPipeLoop(unsigned char * recvline, int connfd, char * fpath) {
    int fd;
    ssize_t n;

    fd = open(fpath, O_RDONLY); /* Abre o pipe para leitura */

    while(1) { /* Loop de leitura do pipe */
        n = read(fd, recvline, MAXLINE);
        write(connfd, recvline, n);
    }
}


void subscribe(unsigned char * recvline, int connfd, char * fpath) {
    static char tname[MAX_TOPIC_LENGTH + 1]; /* Nome do tópicp */
    static char tpath[ROOT_PATH_LENGTH + MAX_TOPIC_LENGTH + 2]; /* Path para o diretório do tópico */
    static char fname[11]; /* Armazena o nome do pipe que será gerado e manipulado ("file name") */

    getTopicName(recvline, tname, 1); /* 1 pois SUBSCRIBE contém Packet Identifier */
    expandPath(tpath, ROOT_PATH, tname); /* Constrói o path para o diretório do tópico */
    createDirectory(tpath); /* Cria o diretório para o tópico, caso não existe */

    randomAlphanumericString(fname, 10); /* Nome do pipe é uma string alfanumérica aleatória */

    expandPath(fpath, tpath, fname); /* Armazena o path para o pipe em fpath */

    if (mkfifo((const char *) fpath,0644) == -1) {
        perror("mkfifo :(\n");
        writeSuback(recvline, connfd, 1); /* Envia SUBACK com falha para cliente */
    }

    writeSuback(recvline, connfd, 0); /* Envia SUBACK com sucesso para o cliente */
}


ssize_t constructRecvline(unsigned char * recvline, int connfd, unsigned char ptype) {
    short int rstart; /* A posição do primeiro byte do restante do pacote ("remaining start") */
    unsigned int rlength; /* Remaining Length (comprimento do restante do pacote) */

    recvline[0] = ptype; /* Insere o primeiro byte no recvline */
    rstart = copyRemainingLength(connfd, recvline); /* Insere os bytes do Remaining Length no recvline */
    rlength = getRemainingLength(recvline); /* Calcula o comprimento restante do pacote a ser lido */

    read(connfd, recvline + rstart, rlength); /* Insere todo o restante do pacote no recvline */

    return rstart + rlength; /* Devolve o COMPRIMENTO TOTAL do pacote */
}


void packetParser(unsigned char ptype, int connfd) {
    static unsigned char recvline[MAXLINE + 1]; /* Armazena o pacote redebido do cliente */
    ssize_t n; /* Armazena o tamanho do pacote recebido do cliente */
    
    static pid_t looppid = -1; /* No subscriber, armazenará o PID do processo que lê o pipe */

    /* Path para o pipe correspondente para o processo de um subscriber */
    static char fpath[ROOT_PATH_LENGTH + MAX_TOPIC_LENGTH + 13]; 
    /* 13 = 10 (comprimento do nome do pipe) + 2 (duas "/" concatenadas no path) + "\0" */
    
    /* Lê todo o pacote, armazenando todos os bytes em recvline, e seu tamanho em n */
    n = constructRecvline(recvline, connfd, ptype);

    switch(recvline[0]) {
        case 0x10: /* CONNECT */
            writeConnack(connfd);
            break;

        case 0x30: /* PUBLISH */
            publish(recvline, n);
            break;

        case 0x82: /* SUBSCRIBE */
            subscribe(recvline, connfd, fpath);
            if((looppid = fork()) == 0)
                readPipeLoop(recvline, connfd, fpath);
            break;

        case 0xc0: /* PINGREQ */
            writePingresp(connfd);
            break;

        case 0xe0: /* DISCONNECT */
            if (looppid > 0) kill(looppid, SIGTERM); /* Encerra o processo filho do subscriber de leitura dos pipes */
            remove(fpath); /* Apaga o pipe correspondente antes de desconectar */
            close(connfd);
            break;

        default:
            printf("Comando MQTT não suportado pelo broker: %02x\n", recvline[0]);
            exit(1);
    }

}