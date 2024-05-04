#ifndef DATA_C
#define DATA_C

/* MQTT Packet Types */
#define UNEXISTING 0 /* not defined by MQTT but helps the ode flow */
#define CONNECT 1
#define CONNACK 2
#define PUBLISH 3
#define PUBACK 4
#define PUBREC 5
#define PUBREL 6
#define PUBCOMP 7
#define SUBSCRIBE 8
#define SUBACK 9
#define UNSUBSCRIBE 10
#define UNSUBACK 11
#define PINGREQ 12
#define PING 13
#define DISCONNECT 14

/* Structs */

struct packet
{
    unsigned char type;
    unsigned char flags;
    unsigned char id_lsb;
    unsigned char id_msb;

    /* Informação relativa a quanto já leu do packet */
    char *raw_bytes;
    /* Informação relativa a quanto já leu do packet */
    long raw_bytes_length;

    /* Informação absoluta do tamanho total do packet */
    long total_raw_bytes_length;
};

struct vector
{
    char *array;
    int length;
};

#endif
