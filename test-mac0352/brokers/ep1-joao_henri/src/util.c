#include "util.h"
#include <stdio.h>

void print_hex(unsigned char* s, int len) {
    for (int i = 0; i < len; ++i)
        printf("%02x ", (unsigned int) *s++);
    printf("\n");
}
