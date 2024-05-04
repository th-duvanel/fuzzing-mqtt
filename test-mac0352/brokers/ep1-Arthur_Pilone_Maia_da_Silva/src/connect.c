#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../include/utilities.h"
#include "../include/connect.h"

/*
 *  Processa mensagem CONNECT vinda de clientes,
 * retornando:
 * 
 *  + -1, se o cabeçalho não merece ser respondido;
 *  +  0, se não forem encontradas falhas no cabeçalho;
 *  +  1, caso o cabeçalho não identificar a versão do protocolo
 *  a ser utilizada como sendo a 3.1.1 (V4);
 *  +  4, caso o cabeçalho conter usuário e / ou senha;
 *  +  5, caso o cliente queira deixar mensagem de 'will' ou 
 *  tenha valor de keepalive > 0;
 * 
 *  Altera, também, o ponteiro apontado por keep_alive_addr para
 * apontar para a eventual duração do keep alive timel, bem como os dados apontados
 * por client_id_addr e client_id_length_addr para apontarem para o client_id dado
 * e seu comprimento, respectivamente.
 */
int process_connect(unsigned char * message, connection_info * con_stats){ // short * keep_alive_addr, char ** client_id_addr, int * client_id_length_addr){
	unsigned char expected_variable_header[6] = {0,4,'M','Q','T','T'};

	if( (message[0] & 15) > 0){
        printf("[Conexão encerrada por não respeitar bits reservados no cabeçalho fixo]\n");
        return -1;
    }

	int remaining_packet_length = decode_remaining_length_adjust_offset(&message[1], & message );
	if(remaining_packet_length < 12){
		printf("[Conexão encerrada por conter cabeçalho de conexão incompleto]\n");
        return -1;
	}

	for(int i = 0; i < 6; i++){
		if(message[i] != expected_variable_header[i]){
			printf("[Conexão encerrada por não se tratar de um protocolo MQTT]\n");
			return -1;
		}
	}
	
	if(message[6] != 4){
		printf("[Um cliente tentou conectar com versão incompatível do protocolo]\n");
		return 1;
	}

	message = &message[7];

	if(message[0] > 63){
		printf("[Um cliente tentou conectar com usuário e / ou senhas]\n");
		return 4;
	}

	if(message[0] > 2){
		printf("[Um cliente tentou conectar com mensagem de Will]\n");
		return 5;
	}

	if(message[0] % 2 == 1){
		printf("[Conexão encerrada por não respeitar bit reservado no byte de flags]\n");
		return -1;
	}

	if(message[0] == 2){
		con_stats->clean_session = 1;
	}else{
		con_stats->clean_session = 0;
	}

	con_stats->keep_alive_time = ( (short) message[1] << 8 | (short) message[2]);

	con_stats->client_id_len = ( (short) message[3] << 8 | (short) message[4]);
	if(con_stats->client_id_len == 0){
		if(con_stats->clean_session == 0){
			printf("Um cliente tentou conectar sem id e sem sessão limpa\n");
			return 2;
		}

		con_stats->client_id_len= int_to_unsigned_string(getpid(),&message[5]);
		con_stats->client_id = &message[5];
	}else{

		con_stats->client_id = & message[5];

		for(int i = 0; i < con_stats->client_id_len;i++){
			if(message[5 + i] == ' '){
				printf("[Um cliente tentou conectar com id contendo ' ']");
				return 2;
			}
		}

	}

	return 0;
}