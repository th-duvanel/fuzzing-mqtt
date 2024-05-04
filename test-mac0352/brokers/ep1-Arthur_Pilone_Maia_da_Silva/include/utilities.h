#define CONTROL_CONNECT     (unsigned char) 1
#define CONTROL_CONNACK     (unsigned char) 2 
#define CONTROL_PUBLISH     (unsigned char) 3 
#define CONTROL_SUBSCRIBE   (unsigned char) 8 
#define CONTROL_SUBACK      (unsigned char) 9 
#define CONTROL_UNSUBSCRIBE (unsigned char) 10
#define CONTROL_UNSUBACK    (unsigned char) 11
#define CONTROL_PINGREQ     (unsigned char) 12
#define CONTROL_PINGRESP    (unsigned char) 13
#define CONTROL_DISCONNECT  (unsigned char) 14

#define LISTENQ 1
#define MAXDATASIZE 100
#define MAXLINE 4096

/*
 *  Ao receber os bytes de `Remaining length' do cabeçalho
 * fixo de uma mensagem em MQTT, decodifica o tamanho em
 * bytes do restante da mensagem.
 * 
 *  Devolve o número de bytes computado, e adianta o 
 * conteúdo do endereço passado como segundo parâmentro para o
 * endereço do primeiro byte do header variável; 
 * 
 *  Código baseado no algoritmo dado na RFC 
 * http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html
 * na seção 2.2.3 .
 */
int decode_remaining_length_adjust_offset(unsigned char *, unsigned char * *);

/*
 *  Ao receber os bytes de `Remaining length' do cabeçalho
 * fixo de uma mensagem em MQTT, decodifica o tamanho em
 * bytes do restante da mensagem.
 * 
 *  Devolve o número de bytes computado, e preenche o endereço
 * de memória passado como segundo parâmetro com o número de
 * bytes que codificavam o campo de `Remaining length'.
 * 
 *  Código baseado no algoritmo dado na RFC 
 * http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html
 * na seção 2.2.3 .
 */
int decode_remaining_length_return_offset(unsigned char *, unsigned short *);

/*
 *  Ao receber um inteiro (comprimento do restante do pacote)
 * e um ponteiro para um vetor de caracteres, codifica o campo
 * `Remaining length' do cabeçalho fixo de uma mensagem em MQTT,
 * e o "escreve" no vetor passado como parâmetro, devolvendo o
 * número de bytes utilizados na codificação.
 * 
 * 
 *  Código baseado no algoritmo dado na RFC 
 * http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html
 * na seção 2.2.3 .
 */
short encode_remaining_length(int, unsigned char *);

char * byte_to_string(char, char *);

unsigned char* append_null_character(unsigned char *, unsigned short);

unsigned short int_to_unsigned_string(int,unsigned char *);