#include <stdio.h>
#include <stdlib.h>
#include <math.h>

/*
 *  Ao receber os bytes de `Remaining length' do cabeçalho
 * fixo de uma mensagem em MQTT, decodifica o tamanho em
 * bytes do restante da mensagem.
 * 
 *  Devolve o número de bytes restantes computado, e adianta o 
 * conteúdo do endereço passado como segundo parâmentro para o
 * endereço do primeiro byte do header variável; 
 * 
 *  Código baseado no algoritmo dado na RFC 
 * http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html
 * na seção 2.2.3 .
 */
int decode_remaining_length_adjust_offset(unsigned char * stream, unsigned char** stream_rest){

	int multiplier = 1;
	int value = 0;
	int index = 0;

	char encodedByte;
	
	do{
    	encodedByte = stream[index];
		index++;
        value += (encodedByte & 127) * multiplier;

        if(multiplier > 128*128*128){
            printf("Error: Malformed Remaining Length in incoming stream");
			return -1;
		}
        
		multiplier = multiplier * 128;
		
	}while ((encodedByte & 128) != 0);

	*stream_rest = &stream[index];

	return value;
}

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
int decode_remaining_length_return_offset(unsigned char * stream, unsigned short* used_bytes){

	int multiplier = 1;
	int value = 0;
	unsigned short index = 0;

	char encodedByte;
	
	do{
    	encodedByte = stream[index];
		index++;
        value += (encodedByte & 127) * multiplier;

        if(multiplier > 128*128*128){
            printf("Error: Malformed Remaining Length in incoming stream");
			return -1;
		}
        
		multiplier = multiplier * 128;
		
	}while ((encodedByte & 128) != 0);

	*used_bytes = index;

	return value;
}

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
short encode_remaining_length(int length, unsigned char * bytes){
	short used_bytes = 0;
	do{

        bytes[used_bytes] = length % 128;

    	length = length / 128;   

    	if ( length > 0 ) // if there are more data to encode, set the top bit of this byte
            bytes[used_bytes] = bytes[used_bytes] | 128;

		used_bytes++;

	}while ( length > 0 );
	return used_bytes;
}

/*
 *  Dados um byte e um ponteiro para uma string de 9 caracteres,
 * gera uma string de 0's e 1's representando o byte dado.
 *
 */
char * byte_to_string(char b, char * byte_as_string){

	for(int i = 0; i < 8; i++){
		byte_as_string[7 - i] = (char) (b & 00000001) + 48 ;
		b = b >> 1;
	}

	byte_as_string[8] = '\0';

	return byte_as_string;
}


/*
 * Devolve uma string (terminada com '\0') que deve ser imediatamente liberada
 */
unsigned char * append_null_character(unsigned char * str, unsigned short len){
	unsigned char * new_str = malloc((len + 1) * sizeof (unsigned char) );
	int i;
	if(new_str == NULL){
		perror("[Programa sem memória !]");
		return NULL;
	}

	for(i = 0; i < len;i++){
		new_str[i] = str[i];
	}

	new_str[len] = '\0';
	return new_str;
}

/*-----------------------------------------------------------*/

unsigned short int_to_unsigned_string(int n,unsigned char * str_dst){
	int len = (int) (floor(log10(n) )+ 1);

	int i = 0;

	unsigned short rest = n;
	i = len-1;
	do{
		unsigned short remainder = rest % 10;
		str_dst[i] = (unsigned char) 48 + remainder;
		rest = (rest - remainder)/10;
		i --;
	}while(rest >= 1);

	str_dst[len] = '\0';

	return len;
}