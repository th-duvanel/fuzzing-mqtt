#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "../include/utilities.h"
#include "../include/data.h"

unsigned short min(unsigned short, unsigned short);

short process_publish(unsigned char * recvline){
	
	// qos = recvline[0] & 6;

	// Checar bits reservados
	if((recvline[0] & 15) != 0){ // Por enquanto, essa linha impede qos > 0, dup = 1 e retain = 1;
		printf("[Pacote PUBLISH do cliente incoerente com protocolo]\n");
		return 1;
	}

	unsigned short i;

	unsigned short packet_len = decode_remaining_length_return_offset(& recvline[1],&i);

	if(packet_len < 2){
		printf("[Pacote PUBLISH do cliente incompleto e/ou incoerente]\n");
		return 1;
	}

	unsigned short topic_name_len = ((unsigned short) recvline[i + 1] << 4 | (unsigned short) recvline[i + 2]);

	if(packet_len - 2 < topic_name_len){
		printf("[Pacote PUBLISH do cliente incompleto e/ou incoerente]\n");
		return 0;
	}

	unsigned short message_len = packet_len - (topic_name_len + 2); // A ser adaptado se vier com packet identifier

	if(message_len > (MAXLINE - i - 4 - topic_name_len) ){
		printf("[Pacote PUBLISH do cliente ultrapassou o limite de tamanho]\n");
		message_len = (MAXLINE - i - 4 - topic_name_len);
	}

	// TODO : a ser adaptado caso incluir qos > 0, devido ao espaço para packet identifier :
	item_index topic_index = get_index_from_index_file(TOPICSTXT,&recvline[i+3],topic_name_len);

	char * topic_subs_path = index_specific_path(TOPICSUBSDIR,topic_index);

	FILE * topics_subs = fopen(topic_subs_path,"rb");

	if(topics_subs == NULL){
		printf("[Não foi possível pegar inscritos do tópico]\n");
	}

	item_index receiver_index;

	while(fread(&receiver_index,sizeof(item_index),1,topics_subs) != 0){
		if(receiver_index == 0){
			fseek(topics_subs,sizeof(unsigned short),SEEK_CUR);
			continue;
		}

		unsigned short clients_max_qos;
		fread(&clients_max_qos,sizeof(unsigned short),1,topics_subs);

		unsigned short effective_qos = clients_max_qos;//min(clients_max_qos,qos) p/ adicionar qos > 0;

		char * receivers_fifo_path = index_specific_path(PIPESDIR,receiver_index);
		int receivers_fifo = open(receivers_fifo_path,O_WRONLY);

		switch(effective_qos){
			case 0:
				
				write(receivers_fifo,recvline,min(MAXLINE,packet_len + 1 + i));
				break;
			
			default:
				close(receivers_fifo);
				free(receivers_fifo_path);

				return 1;
		}

		close(receivers_fifo);
		free(receivers_fifo_path);
		
	}

	free(topic_subs_path);
	fclose(topics_subs);

	return 0;
}

unsigned short min(unsigned short a, unsigned short b){
	if(a < b)
		return a;
	else
		return b;
}