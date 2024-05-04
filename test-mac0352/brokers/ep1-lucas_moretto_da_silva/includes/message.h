#ifndef MESSAGE_H
#define MESSAGE_H

#include <util.h>

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include <constants.h>

typedef struct {
    int packetType;
    int flags;
    int remainingSize;
    char * remaining;
} Message;

#endif
