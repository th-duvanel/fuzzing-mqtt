#ifndef UTIL_H
#define UTIL_H

#include <constants.h>

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

char *byteToBinary(
        char *dest,
        const char *src,
        int n,
        size_t startPoint
        );

void matchValues(
        int val,
        int expectedVal,
        int errorCode
        );

void setSignIntAction();

void resetSignIntAction();

int getTopicPipe(
        char *topicPath,
        mode_t mode
        );

#endif
