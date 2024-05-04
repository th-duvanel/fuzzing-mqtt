#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "auxiliar.h"

void toBit (char c, int v[]) {
    int i;
    for (i = 0; i < 8; i ++){
        v[i] = !!((c << i) & 0x80);
    }
}

int calculaBit (int byte[], int limite) {
    int x = 0, i;
    for (i = 0; i < limite; i ++) 
        x += byte[i] * pow (2, limite - i - 1);
    
    return x;
}

void leStringPacote (int ini, int fim, char recvline[], char * topico) {
    int byte[8];
    int x, j;
    char c;
    for (j = ini; j < fim; j ++) {
        toBit (recvline[j], byte);
        x = calculaBit (byte, 8);
        c = x;
        strncat (topico, &c, 1);
    }
}