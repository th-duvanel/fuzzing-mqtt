#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/utilities.h"
#include "../include/data.h"

short process_unsubscribe(unsigned char * recvline, int connfd, item_index client_index){
	if((recvline[0] & 15) != 2){ //Checar bits reservados
		return 1; 
	}

	unsigned short i;

	unsigned short packet_len = decode_remaining_length_return_offset(& recvline[1],&i);
	unsigned char packet_identifier[2] = {recvline[i + 1], recvline[i + 2]};
	unsigned short remaining_len = packet_len;

	if(packet_len <= 2){ // Violação de protocolo [MQTT-3.10.3-2]
		return 1;
	}

	if(packet_len > (MAXLINE - 8)){
		perror("[Pacote UNSUBSCRIBE do cliente ultrapassou o limite de tamanho]\n");
		remaining_len = (MAXLINE - 8);
	}

	i+= 3;

	unsigned short next_topic_len = ( (unsigned short) recvline[i] << 8 | (unsigned short) recvline[i+1]);

	while(next_topic_len <= remaining_len){
		unsubscribe(client_index,&recvline[i+ 2],next_topic_len);

		i += 2 + next_topic_len;
		remaining_len -= 2 + next_topic_len;
		if(remaining_len <= 2){
			break;
		}
		
		next_topic_len = ( (unsigned short) recvline[i] << 8 | (unsigned short) recvline[i+1]);
	}

	unsigned char unsub_response[4] = {(unsigned char) (CONTROL_UNSUBACK << 4),2, packet_identifier[0],packet_identifier[1]};
	write(connfd,unsub_response,4);

	return 0;
}