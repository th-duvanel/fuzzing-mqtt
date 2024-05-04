#ifndef FUNCTIONS_H
#define FUNCTIONS_H

#include <sys/types.h>

/* Path para o diretório raiz do broker em /tmp */
#define ROOT_PATH "/tmp/mac0352-11795888"
#define ROOT_PATH_LENGTH 21

/* Máximo de 65536 bytes do comprimento do nome do tópico*/
#define MAX_TOPIC_LENGTH 65536

/* Máximo de 5 bytes no Fixed Header + máximo de 268435455 bytes do restante do pacote */
#define MAXLINE 268435460 

void createRootDirectory();
void cleanRootDirectory();

void packetParser(unsigned char ptype, int connfd);

#endif