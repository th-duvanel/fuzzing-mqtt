#ifndef UTILS_C
#define UTILS_C

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "data.c"

/* Function Headers */
unsigned int construct_int(unsigned char low, unsigned char high);
int last_index_of(char *str, char c);
char *copy_str(char *src, int n);

void print_bytes(char *bytes, int length);
void print_binary(unsigned int number);

/* ========================================================= */
/*               AUXILIAR FUNCTION DEFINITIONS               */
/* ========================================================= */

/**
 * Código adapdado do Stack Overflow feito por <roylewilliam>
 * (referência: https://stackoverflow.com/a/70930112/11317116).
 * Prints the binary representation of any unsigned integer.
 * **/
void print_binary(unsigned int number)
{
    if (number >> 1)
    {
        print_binary(number >> 1);
        putc((number & 1) ? '1' : '0', stdout);
    }
    else
    {
        putc((number & 1) ? '1' : '0', stdout);
    }
}

void print_bytes(char *bytes, int length)
{
    for (int i = 0; i < length; i++)
        printf("%d(%c) ", (int)bytes[i], bytes[i]);
    printf("\n");
}

/* ========================================================= */
/*                   FUNCTION DEFINITIONS                    */
/* ========================================================= */

unsigned int construct_int(unsigned char lsb, unsigned char msb)
{
    unsigned int mult = 1;
    for (int i = 0; i < CHAR_BIT; i++)
        mult *= 2;
    return lsb + (msb * mult);
}

char *copy_str(char *src, int n)
{
    char *cpy = (char *)malloc((n + 1) * sizeof(char));
    memcpy((void *)cpy, (void *)src, n);
    cpy[n] = '\0';
    return cpy;
}

int last_index_of(char *str, char c)
{
    int length = strlen(str);
    int index_of = -1;
    for (int i = 0; i < length; i++)
        if (str[i] == c)
            index_of = i;
    return index_of;
}

#endif
