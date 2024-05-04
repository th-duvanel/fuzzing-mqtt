#include <stdio.h>
#include <stdlib.h>

typedef struct cel
{
    int file_number;
    struct cel *prox;
} celula;

celula *insereNoFim(celula *inicio, int PID);
celula *insereNoInicio(celula *inicio, int PID);
celula *removePrimeiro(celula *inicio);
celula *removeCelula(celula *inicio, int PID);
celula *freeLista(celula *inicio);

void *malloc_global(size_t size);
void free_global(void *addr, size_t size);

int findTopic(char **topicsList, char *topic);
int findSpot(char **topicsList);
