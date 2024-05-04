#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <math.h>
#include <unistd.h>

#include <string.h>

#include "../include/data.h"

#include "../include/utilities.h"

/*-----------------------------------------------------------*/
/*              Private Functions Declaration                */
/*-----------------------------------------------------------*/

unsigned short add_entry_to_subs_file(char *, item_index, unsigned short);

void remove_entry_from_subs_file(char *, item_index);

/*-----------------------------------------------------------*/

item_index get_index_from_index_file(char *, unsigned char *, unsigned short);

item_index add_entry_to_index_file(char *, unsigned char *, unsigned short);

/*-----------------------------------------------------------*/

unsigned short fread_index_and_stringf(FILE *,item_index *,unsigned char ** );

short compare_strings(unsigned char *, unsigned short,unsigned char *, unsigned short);

short compare_string_to_id(unsigned char *,unsigned char *, unsigned short);

/*-----------------------------------------------------------*/

char * index_specific_path(char *,item_index);

/*-----------------------------------------------------------*/
/*                  Functions Definitions                    */
/*-----------------------------------------------------------*/

/*
 *  Inscreve o cliente de índice passado no tópico dado,
 * identificado por seu id e seu comprimento;
 * 
 *  Se o cliente ja estiver inscrito no tópico, o deixa
 * inscrito com qos máximo sendo o mínimo entre o passado 
 * e o antigo. Esse qos é o valor devolvido pela função.
 */
unsigned short subscribe(item_index client_index,unsigned char * topic_id, unsigned short topic_id_len,unsigned short requested_qos){
	item_index topic_index;
	

	// Achar index do tópico ou criar se ele ainda n existe;
	if(access(TOPICSTXT, F_OK) == 00){
		topic_index = get_index_from_index_file(TOPICSTXT,topic_id,topic_id_len);

		if(topic_index == 0){
			topic_index = add_entry_to_index_file(TOPICSTXT,topic_id,topic_id_len);

			char * topic_subs_path = index_specific_path(TOPICSUBSDIR,topic_index);

			fclose(fopen(topic_subs_path,"a"));

			free(topic_subs_path);
		}

	}else{
		FILE * topics = fopen(TOPICSTXT,"a");

		if(topics == NULL){
			perror("[Não foi possível abrir arquivo necessário para criar tópico]\n");
		}

		unsigned char * topic_id_as_str = append_null_character(topic_id,topic_id_len);
		char * topic_subs_path = index_specific_path(TOPICSUBSDIR,1);

		fprintf(topics,"1 %s \n",topic_id_as_str);

		fclose(fopen(topic_subs_path,"a"));

		free(topic_id_as_str);
		free(topic_subs_path);

		fclose(topics);

		topic_index = 1;
	}

	// Registrar a inscrição

	char * client_info_path = index_specific_path(CLIENTSDIR,client_index);
	char * topic_subs_path = index_specific_path(TOPICSUBSDIR,topic_index);

	unsigned short best_qos = add_entry_to_subs_file(client_info_path,topic_index,requested_qos);
	add_entry_to_subs_file(topic_subs_path,client_index, best_qos);

	free(client_info_path);
	free(topic_subs_path);
	
	return best_qos;
}


/*
 * Remove a insctrição de um cliente em um tópico.
 */
void unsubscribe(item_index client_index,unsigned char * topic_id, unsigned short topic_id_len){
	if(access(TOPICSTXT, F_OK) == 00){
		item_index topic_index = get_index_from_index_file(TOPICSTXT,topic_id,topic_id_len);

		if(topic_index != 0){
			char * client_info_path = index_specific_path(CLIENTSDIR,client_index);
			char * topic_subs_path = index_specific_path(TOPICSUBSDIR,topic_index);

			remove_entry_from_subs_file(topic_subs_path,client_index);
			remove_entry_from_subs_file(client_info_path,topic_index);

			free(client_info_path);
			free(topic_subs_path);
		}
	}
}

/*
 *  Função responsável por efetivar a conexão de um cliente,
 * permitindo que ele volte a receber mensagens de tópicos
 * inscritos, caso ele já exista, ou criando um novo arquivo, 
 * caso se trate de um cliente novo;
 * 
 *  Recebe o client_id e seu comprimento, devolve um identifi-
 * cador único e pequeno (client_index) reutilizado durante
 * a conexão.
 */
item_index connect_client(unsigned char * client_id, unsigned short client_id_len, unsigned short * ses_present_addr){
	FILE * clients;
	char * client_info_path;
	char * client_fifo_path;
	if(access(CLIENTIDSTXT, F_OK) == 00){

		item_index client_index = get_index_from_index_file(CLIENTIDSTXT,client_id,client_id_len);

		if(client_index == 0){ // Não tinha info sobre esse cliente
			client_index = add_entry_to_index_file(CLIENTIDSTXT,client_id,client_id_len);

			client_info_path = index_specific_path(CLIENTSDIR ,client_index);
			client_fifo_path = index_specific_path(PIPESDIR,client_index);

			*ses_present_addr = 0;

			fclose(fopen(client_info_path,"a")); // Crio seu arquivo de inscrições
		}else{
			client_info_path = index_specific_path(CLIENTSDIR ,client_index);
			client_fifo_path = index_specific_path(PIPESDIR,client_index);

			if(access(client_info_path, F_OK) == 00){ // Tenho info sobre os tópicos em que ele era estava inscrito
				FILE * client_file = fopen(client_info_path,"rb"); 

				item_index topic_index;
				while(fread(&topic_index,sizeof(item_index),1,client_file) != 0){
					unsigned short topic_sub_qos;

					fread(&topic_sub_qos,sizeof(unsigned short),1,client_file);

					if(topic_index == 0){
						continue;
					}

					char * topic_subs_path = index_specific_path(TOPICSUBSDIR, topic_index);

					add_entry_to_subs_file(topic_subs_path,client_index,topic_sub_qos);

					free(topic_subs_path);
				}

				*ses_present_addr = 1;

				fclose(client_file);
			}else{
				*ses_present_addr = 0;
				fclose(fopen(client_info_path,"a")); // Crio seu arquivo de inscrições
			}
		}

		if(mkfifo((const char *) client_fifo_path,0644) == -1){
			perror("fifo :(\n");
		}

		free(client_fifo_path);
		free(client_info_path);
		return client_index;

	}else{

		mkdir(ROOTDIR,0664);

		clients = fopen(CLIENTIDSTXT,"a");
		if(clients == NULL){
			perror("[Não foi possível abrir arquivo necessário para conectar cliente]\n");
		}

		mkdir(CLIENTSDIR,0664);
		mkdir(TOPICSUBSDIR,0664);
		mkdir(PIPESDIR,0664);

		unsigned char * client_id_str = append_null_character(client_id,client_id_len);

		char * client_info_path = index_specific_path(CLIENTSDIR ,(unsigned short)1);

		client_fifo_path = index_specific_path(PIPESDIR,(unsigned short) 1);
		
		fprintf(clients,"1 %s \n",client_id_str);

		fclose(clients);

		fclose(fopen(client_info_path,"a"));

		mkfifo((const char *) client_fifo_path,0644);

		free(client_fifo_path);
		free(client_info_path);
		free(client_id_str);

		*ses_present_addr = 0;

		return 1;
	}
	
}

/*
 *  Função responsável por efetivar a desconexão de um cliente.
 * Na prática, remove referências a ele nos tópicos em que ele é
 * inscrito.
 */
void disconnect_client(item_index client_index){
	char * client_info_path = index_specific_path( CLIENTSDIR, client_index);
	char * client_fifo_path = index_specific_path( PIPESDIR, client_index);

	FILE * client_subs = fopen(client_info_path,"rb"); 

	item_index topic_index;
	while(fread(&topic_index,sizeof(item_index),1,client_subs) == 1){

		fseek(client_subs,sizeof(unsigned short),SEEK_CUR);

		if(topic_index == 0){
			continue;
		}

		char * topic_index_as_str = index_specific_path(TOPICSUBSDIR,topic_index);

		remove_entry_from_subs_file(topic_index_as_str,client_index);

		free(topic_index_as_str);
	}

	unlink((const char *) client_fifo_path);

	free(client_info_path);
	free(client_fifo_path);

	fclose(client_subs);
}

/*
 *  Função responsável por apagar as informações armazenadas sobre
 * um dado cliente. Vale notar, entretanto, que uma das limitações
 * do código como ele está é que o índice interno (index) designado
 * a ele *não* é liberado para uso por outro cliente, criando um
 * limite de no máximo (unsigned short max) - 1 clientes de diferentes
 * client_id's conectados ao servidor durante uma execução. 
 */
void clear_client_info(item_index client_index){
	char * client_info_path = index_specific_path(CLIENTSDIR, client_index);

	disconnect_client(client_index);

	fclose(fopen(client_info_path,"w+"));

	free(client_info_path);
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

/*
 *  Adiciona entrada (par item_index, QoS) a arquivo que armazena
 * inscrições - seja de um tópico, ou de um cliente.
 * 
 *  Supõe que o arquivo referenciado existe e, caso já exista uma
 * entrada para o dado item_index no arquivo, é mantida e devolvido
 * o qos mínimo.
 * 
 *  Caso não seja encontrada entrada no arquivo com o item_index 
 * passado, tenta trocar entrada vazia (item_index = 0) pela nova
 * entrada e, caso não seja encontrada uma entrada vazia, uma nova
 * entrada é adicionada no final do arquivo.
 * 
 */
unsigned short add_entry_to_subs_file(char * arq_path, item_index index, unsigned short qos){
	FILE * subs = fopen(arq_path,"rb+");

	item_index index_read;
	unsigned short qos_read;

	while(fread(&index_read,sizeof(item_index),1,subs) == 1){
		if(index_read == 0){

			fseek(subs,-1*sizeof(item_index),SEEK_CUR);

			fwrite(&index,sizeof(item_index),1,subs);
			fwrite(&qos,sizeof(unsigned short),1,subs);

			fclose(subs);
			return qos;
		}else if(index_read == index){
			fread(&qos_read,sizeof(unsigned short),1,subs);
			if(qos_read < qos){
				fclose(subs);
				return qos_read;
			}
			fseek(subs,-1*sizeof(unsigned short),SEEK_CUR);
			fwrite(&qos,sizeof(unsigned short),1,subs);
			fclose(subs);
			return qos;
		}
		fread(&qos_read,sizeof(unsigned short),1, subs);
	}

	fclose(subs);
	subs = fopen(arq_path,"ab");

	fwrite(&index,sizeof(item_index),1,subs);
	fwrite(&qos,sizeof(unsigned short),1,subs);

	fclose(subs);

	return qos;

}
/*
 *  Remove entrada (par item_index, QoS) de arquivo que armazena
 * inscrições - seja de um tópico, ou de um cliente.
 * 
 *  Supõe que o arquivo existe e caso seja encontrada entrada
 * com o item_index passado, este é trocado por 0 no arquivo, 
 * a fim de indicar a ausência de uma entrada na lista.
 * 
 */
void remove_entry_from_subs_file(char * arq_path,item_index index){
	FILE * subs = fopen(arq_path,"rb+");

	item_index index_read;
	item_index null_index = 0;

	while(fread(&index_read,sizeof(item_index),1,subs) == 1){
		if(index_read == index){
			fseek(subs,-1*sizeof(item_index),SEEK_CUR);
			fwrite(&null_index,sizeof(item_index),1,subs);
			break;
		}
	}

	fclose(subs);
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

/*
 *  Dada uma string (um client_id ou um topic_name), e um arquivo 
 * que contenha relações (item_index - string), busca o item_index
 * associado à dada string.
 * 
 * Supõe que o arquivo passado existe e segue o formato esperado. 
 * Devolve 0 caso não seja encontrada entrada com a string dada,
 * e o seu item_index caso a entrada seja encontrada.
 */
item_index get_index_from_index_file(char * arq_path, unsigned char * name, unsigned short name_len){
	FILE * arq = fopen(arq_path,"r");

	if(arq == NULL){
		perror("Não foi possível acessar arquivo vital à execução\n");
		exit(7);
	}

	item_index index_read;
	unsigned char * name_read;
	unsigned short name_read_len = fread_index_and_stringf(arq,&index_read,&name_read);


	while(name_read_len > 0){
		if( compare_strings(name_read,name_read_len,name,name_len) == 0){
			free(name_read);
			fclose(arq);
			return index_read;
		}
		free(name_read);
		name_read_len = fread_index_and_stringf(arq,&index_read,&name_read);
	}

	free(name_read);
	fclose(arq);
	return 0;
}

/*-----------------------------------------------------------*/

/*
 *  Adiciona entrada (item_index - string) ao arquivo passado
 * caso ele exista e possua o formato esperado.
 * 
 *  Devolve o item_index atribuído à string dada.
 */
item_index add_entry_to_index_file(char * arq_path, unsigned char * name, unsigned short name_len){
	FILE * arq = fopen(arq_path,"r");

	item_index last_index = 0;
	char c;

	while( fscanf(arq,"%hu ",&last_index) == 1){
		
		do{
			if(fscanf(arq,"%c", &c) == 0 )
				break;
		}while(c != '\n');
		
	}

	fclose(arq);
	arq = fopen(arq_path,"a");

	unsigned char * name_str = append_null_character(name,name_len);

	fprintf(arq,"%hu %s \n",last_index + 1, name_str);

	free(name_str);

	fclose(arq);

	return last_index + 1;
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

/*
 *  Função responsável por ler um item_index e uma string de
 * um arquivo dado.
 * 
 *  O item_index e a string são "devolvidas" alterando os 
 * ponteiros passados como parâmetros, enquanto que o tamanho
 * da string lida é devolvida como o valor de retorno da função.
 * 
 * <IMPORTANTE> : É necessário liberar o char * que armazena a string;
 */
unsigned short fread_index_and_stringf(FILE * arq,item_index * id_addr,unsigned char ** string_addr){
	unsigned short len = 0;
	unsigned short max_len = 32;

	if( fscanf(arq,"%hu ",id_addr) == 0){
		perror("Tentei ler de arquivo corrompido ou incompleto\n");
		return 0;
	}

	unsigned char * str_read = malloc(max_len*sizeof(unsigned char));

	int c = fgetc(arq);
	int i;

	while(c != EOF && c != '\0' && c != ' ' && c!= '\n'){
		if(len >= max_len){
			unsigned char * new_str = malloc(2*max_len*sizeof(unsigned char));

			for(i = 0 ; i < max_len; i++){
				new_str[i] = str_read[i];
			}

			free(str_read);
			str_read = new_str;
			max_len *= 2;
		}
		str_read[len] = (unsigned char) c;
		len ++;
		c = fgetc(arq);
	}

	*string_addr = str_read;
	return len;
}

/*-----------------------------------------------------------*/

/*
 * Compara duas strings de tamanhos conhecidos, retornando
 * -1 se a primeira string é "menor" que a segunda
 *  0 se elas forem iguais
 *  1 se a primeira string é "maior" que a segunda
 */
short compare_strings(unsigned char * str1, unsigned short len1,unsigned char * str2, unsigned short len2){
	int min, smaller_str, i;
	if(len1 < len2){
		min = len1;
		smaller_str = -1;
	}else{
		min = len2;
		smaller_str = 1;
	}

	unsigned char * str1_strmesmo = append_null_character(str1,len1);
	unsigned char * str2_strmesmo = append_null_character(str2,len2);

	free(str1_strmesmo);
	free(str2_strmesmo);

	for(i = 0; i < min; i++){
		unsigned char c1, c2;
		c1 = str1[i];
		c2 = str2[i];

		if(c1 < c2){
			return -1;
		}else if(c1 > c2){
			return 1;
		}
	}

	if(len1 == len2){
		return 0;
	}else{
		return smaller_str;
	}
}

/*-----------------------------------------------------------*/

short compare_string_to_id(unsigned char * str,unsigned char * id, unsigned short id_len){
	int i;

	for(i = 0; i < id_len; i++){
		unsigned char c1, c2;
		c1 = str[i];
		c2 = id[i];

		if(c1 < c2 || c1 == '\0'){
			return -1;
		}else if(c2 > c1){
			return 1;
		}
	}

	if(str[id_len] == '\0'){
		return 0;
	}else{
		return 1;
	}
}

/*-----------------------------------------------------------*/
/*-----------------------------------------------------------*/

/*
 *  Dados um caminho e um item_index, devolve uma string com o
 * o caminho para um arquivo no diretório apontado por <path>
 * e de nome igual ao item_index passado.
 * 
 *  Resultado deve ser eliminado / liberado
 */
char * index_specific_path(char * path,item_index index){
	int len = (int) (floor(log10(index) ) + 2);
	len += strlen(path);

	char * new_path = malloc(len*sizeof(char));

	int i = 0;

	while(i < strlen(path)){
		new_path[i] = path[i];
		i++;
	}

	unsigned short rest = index;
	i = len-2;
	do{
		unsigned short remainder = rest % 10;
		new_path[i] = 48 + remainder;
		rest = (rest - remainder)/10;
		i --;
	}while(rest >= 1);

	new_path[len-1] = '\0';

	return new_path;
} 

/*-----------------------------------------------------------*/