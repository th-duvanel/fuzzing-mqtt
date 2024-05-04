#ifndef QUEUE_EP1
#define QUEUE_EP1
#include <stdlib.h>

#define QUEUE_SUCCESS 1
#define QUEUE_FAILURE 0

struct Cell {
    struct Cell *next;
    struct Cell *prev;
    void *value;
};

struct Queue {
    struct Cell *first;
    void (*free)(void *);
    ssize_t length;
};

struct Queue *queueCreate (void (*free)(void *));
ssize_t queueLength (struct Queue *queue);
int queueIsEmpty (struct Queue *queue);
int queuePush (struct Queue *queue, void *value);
void *queuePop (struct Queue *queue);
void queuePurge (struct Queue *queue);

#endif
