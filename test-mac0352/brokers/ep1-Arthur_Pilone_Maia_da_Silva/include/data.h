#include <math.h>

#define CLIENTIDSTXT      "/tmp/mac352ep1AP/clients.txt"
#define TOPICSTXT         "/tmp/mac352ep1AP/topics.txt"
#define CLIENTSDIR        "/tmp/mac352ep1AP/clients/"
#define TOPICSUBSDIR      "/tmp/mac352ep1AP/topics/"
#define PIPESDIR          "/tmp/mac352ep1AP/pipes/"
#define ROOTDIR           "/tmp/mac352ep1AP/"

typedef unsigned short item_index;

/*
 *  Inscreve o cliente de índice passado no tópico dado,
 * identificado por seu id e seu comprimento;
 * 
 *  Se o cliente ja estiver inscrito no tópico, o deixa
 * inscrito com qos máximo sendo o mínimo entre o passado 
 * e o antigo. Esse qos é o valor devolvido pela função.
 */
unsigned short subscribe(item_index,unsigned char *, unsigned short, unsigned short);

/*
 * Remove a insctrição de um cliente em um tópico.
 */
void unsubscribe(item_index,unsigned char *, unsigned short); /* Recebem nome do cliente e tópico*/

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
item_index connect_client(unsigned char *, unsigned short, unsigned short *);

/*
 *  Função responsável por efetivar a desconexão de um cliente.
 * Na prática, remove referências a ele nos tópicos em que ele é
 * inscrito.
 */
void disconnect_client(item_index); /* Recebe índice do cliente*/

/*
 *  Função responsável por apagar as informações armazenadas sobre
 * um dado cliente. Vale notar, entretanto, que uma das limitações
 * do código como ele está é que o índice interno (index) designado
 * a ele *não* é liberado para uso por outro cliente, criando um
 * limite de no máximo (unsigned short max) - 1 clientes de diferentes
 * client_id's conectados ao servidor durante uma execução. 
 */
void clear_client_info(item_index); /* Recebe índice do cliente*/

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
char * index_specific_path(char *,item_index);


/*
 *  Dada uma string (um client_id ou um topic_name), e um arquivo 
 * que contenha relações (item_index - string), busca o item_index
 * associado à dada string.
 * 
 * Supõe que o arquivo passado existe e segue o formato esperado. 
 * Devolve 0 caso não seja encontrada entrada com a string dada,
 * e o seu item_index caso a entrada seja encontrada.
 */
item_index get_index_from_index_file(char *, unsigned char *, unsigned short);