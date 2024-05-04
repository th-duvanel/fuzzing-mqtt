#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/utilities.h"
#include "../include/data.h"

short process_subscribe(unsigned char * recvline, int connfd, item_index client_index){
	if((recvline[0] & 15) != 2){ //Checar bits reservados
		return 1;
	}

	unsigned char sub_qos[MAXDATASIZE];
	unsigned short i;

	unsigned short packet_len = decode_remaining_length_return_offset(& recvline[1],&i);
	unsigned char packet_identifier[2] = {recvline[i + 1], recvline[i + 2]};
	unsigned short remaining_len = packet_len;

	unsigned short no_topic_requests = 0;

	if(packet_len <= 2){ // Violação de protocolo [MQTT-3.8.3-3]
		return 1;
	}
	if(packet_len > (MAXLINE - 8)){
		perror("[Pacote SUBSCRIBE do cliente ultrapassou o limite de tamanho]\n");
		remaining_len = (MAXLINE - 8);
	}

	i+=3;

	unsigned short next_topic_len = ( (unsigned short) recvline[i] << 8 | (unsigned short) recvline[i+1]);

	while(next_topic_len <= remaining_len){
		sub_qos[no_topic_requests] = (unsigned char) subscribe(client_index,&recvline[i+ 2],next_topic_len, 0);

		//sub_qos[no_topic_requests] = (unsigned char) subscribe(client_index,&recvline[i+ 2],next_topic_len,(unsigned short) recvline[i+2 + next_topic_len]); Para qos != 0

		if((recvline[i+2 + next_topic_len] & 3) == 3){
			return 1;
		}

		no_topic_requests++;

		i += 3 + next_topic_len;
		remaining_len -= 3 + next_topic_len;
		if(remaining_len <= 2){
			break;
		}
		
		next_topic_len = ( (unsigned short) recvline[i] << 8 | (unsigned short) recvline[i+1]);
	}

	unsigned char * sub_response = malloc(8 + no_topic_requests);
	short pack_len_len = encode_remaining_length(no_topic_requests + 2,&sub_response[1]);
	int j;

	sub_response[0] = (CONTROL_SUBACK << 4);
	
	sub_response[1 + pack_len_len] = packet_identifier[0];
	sub_response[2 + pack_len_len] = packet_identifier[1];

	for(j = 0; j < no_topic_requests;j++){
		sub_response[j + pack_len_len + 3] = sub_qos[j];
	}

	write(connfd, sub_response,j + pack_len_len + 3);

	free(sub_response);

	return 0;
}