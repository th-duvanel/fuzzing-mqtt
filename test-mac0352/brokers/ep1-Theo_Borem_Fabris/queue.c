#include "queue.h"

struct Queue *queueCreate(void (*free)(void *))
{
    struct Queue *queue;

    if ((queue = malloc(sizeof(struct Queue))) == NULL)
        return NULL;
    queue->first = NULL;
    queue->length = 0;
    queue->free = free;
    return queue;
}


ssize_t queueLength(struct Queue *queue)
{
    if (queue == NULL)
        return -1;
    return queue->length;
}

int queueIsEmpty(struct Queue *queue)
{
    if (queue == NULL)
        return 0;
    return (queue->length == 0);
}

void *queuePop(struct Queue *queue)
{
    struct Cell *temp;
    void *value;

    if (queue == NULL || queue->length == 0)
        return NULL;

    if (queue->length == 1) {
        value = queue->first->value;
        free(queue->first);
        queue->first = NULL;
    }
    else {
        temp = queue->first;
        value = temp->value;
        queue->first = temp->next;
        temp->prev->next = temp->next;
        temp->next->prev = temp->prev;
        free(temp);
    }
    queue->length--;
    return value;
}

int queuePush(struct Queue *queue, void *value)
{
    struct Cell *new, *last;

    if ((new = malloc(sizeof(struct Cell))) == NULL)
        return QUEUE_FAILURE;
    new->value = value;
    if (queue->first == NULL) {
        queue->first = new;
        new->next = new;
        new->prev = new;
    }
    else {
        last = queue->first->prev;
        queue->first->prev = new;
        new->next = queue->first;
        last->next = new;
        new->prev = last;
    }
    queue->length++;
    return QUEUE_SUCCESS;
}

void queuePurge(struct Queue *queue)
{
    struct Cell *temp, *next;
    if (queue == NULL || queue->first == NULL) {
        free(queue);
        return;
    }
    next = queue->first->next;
    while (next != queue->first) {
        temp = next->next;
        if (queue->free != NULL)
            (*queue->free)(next->value);
        free(next);
        next = temp;
    }
    if (queue->free != NULL)
        (*queue->free)(next->value);
    free(next);
    free(queue);
}
