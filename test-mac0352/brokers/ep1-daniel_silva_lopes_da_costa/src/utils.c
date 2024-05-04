
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <errno.h>
#include "utils.h"
#include "definitions.h"


/* 

Insere no inicio da lista

*/

celula *insereNoInicio(celula *inicio, int PID)
{
  celula *novo = malloc_global(sizeof(celula));
  novo->file_number = PID;
  novo->prox = inicio;
  return (novo);
}

/* 

Insere no fim da lista

*/

celula *insereNoFim(celula *inicio, int PID)
{
  celula *novo = malloc_global(sizeof(celula));
  celula *atual, *ant;
  novo->file_number = PID;
  novo->prox = NULL;
  for (atual = inicio, ant = NULL; atual != NULL;
       ant = atual, atual = atual->prox)
  if (ant != NULL)
    ant->prox = novo;
  else
    inicio = novo;
  return (inicio);
}

/* 

celula * removePrimeiro (celula * inicio); 

*/

celula *removePrimeiro(celula *inicio)
{
  celula *aux = inicio;
  if (inicio != NULL)
  {
    inicio = inicio->prox;
    free_global(aux, sizeof(celula));
  }
  return inicio;
}

/* 

Remove uma ocorrência de x da lista apontada por inicio 
celula * remove (celula * inicio, int x); 

*/

celula *removeCelula(celula *inicio, int PID)
{
  celula *aux = inicio;
  if (inicio == NULL)
    return inicio;
  if (inicio->file_number == PID)
  {
    inicio = inicio->prox;
    free_global(aux, sizeof(celula));
  }
  else
    inicio->prox = removeCelula(inicio->prox, PID);
  return (inicio);
}

celula *freeLista(celula *inicio)
{
  while (inicio != NULL)
  {
    inicio = removePrimeiro(inicio);
  }
}

/* 

Reserva memória global que pode ser acessado por todos os processos

*/

void *malloc_global(size_t size)
{
  void *allocated_bytes = mmap(NULL, size, PROT_READ | PROT_WRITE,
                               MAP_SHARED | MAP_ANONYMOUS, 0, 0);
  if (allocated_bytes == MAP_FAILED)
  {
    fprintf(stderr,
            "Failed to allocate the bytes with mmap.\nErrno = "
            "%d.\nExiting...\n",
            errno);
    exit(errno);
  }

  return allocated_bytes;
}

/* 

Libera  memória global

*/

void free_global(void *addr, size_t size)
{
  int err = munmap(addr, size);

  if (err != 0)
  {
    fprintf(stderr, "Um erro ocorreu ao tentar desalocar um mmap\n");
    exit(errno);
  }
}



/* 

Encotra o identificador de um determinado tópico - 
volta -1 se o tópico não tiver subscribers

*/

int findTopic(char **topicsList, char *topic)
{
    for (int i = 0; i < MAX_NUMBER_TOPICS; i++)
    {
        if (strlen(topicsList[i]) > 0)
        {
            if (strcmp(topicsList[i], topic) == 0)
            {
                return i;
            }
        }
    }
    return -1;
}


/* 

Encotra o identificador vago para um determinado tópico
Volta -1 se o limite de tópicos tiver sido atingido.

*/

int findSpot(char **topicsList)
{
    for (int i = 0; i < MAX_NUMBER_TOPICS; i++)
    {
        if (strlen(topicsList[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}