#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#define LISTENQ 1

/* ---------------- Auxiliary constants, structs and funcions----------------*/

static const char ROUTER_FILE[] = "/tmp/temp.mac0352.ROUTER";
static const char ROUTER_LOCK[] = "/tmp/temp.mac0352.ROUTER.lock";

typedef struct packet{
	uint8_t type;
	int length;
	uint8_t* contents;
} packet;

typedef struct subscriptionNode{
	uint16_t topicSize;
	char* topic;
	uint16_t processPipeID;
	struct subscriptionNode* nextNode;
} subscriptionNode;

int equalTopics (char* topic1, char* topic2, int topicSizes) {
	int index = 0;

	while (index < topicSizes && topic1[index] == topic2[index]) {
		index++;
	}

	if (index == topicSizes) return 1;
	return 0;
}

/* --------------------------- Main functions --------------------------- */

void notifyClient (subscriptionNode* node, char* message, uint32_t messageSize) {
	char workerPipeString[100];
	sprintf(workerPipeString, "/tmp/temp.mac0352.WORKER.%d", node->processPipeID);
	char workerPipeStringReady[100];
	sprintf(workerPipeStringReady, "/tmp/temp.mac0352.WORKER.%d.ready", node->processPipeID);

	char command[256];
	snprintf(command, sizeof command, "touch %s", workerPipeStringReady);
	system(command);

	int clientFD = open(workerPipeString, O_WRONLY);

	char messageHeader = 'M';

	write(clientFD, &messageHeader, sizeof(char));
	write(clientFD, &node->topicSize, sizeof(node->topicSize));
	write(clientFD, &messageSize, sizeof(messageSize));
	write(clientFD, node->topic, node->topicSize);
	write(clientFD, message, messageSize);

	close(clientFD);
}

subscriptionNode* ROUTER_handleSubscribe(subscriptionNode* startingNode, int routerPipeFd) {
	uint16_t currentTopicSize;
	uint16_t workerPipeNumber;
	subscriptionNode* newNode = NULL;
	subscriptionNode* currentIndexNode = NULL;

	/* reads 2-byte topic length, then reads 4-byte message size */

	/* read topic size and workerID from WORKER process */
	read(routerPipeFd, &currentTopicSize, sizeof(currentTopicSize));
	read(routerPipeFd, &workerPipeNumber, sizeof(workerPipeNumber));

	/* generate new node with according size */
	newNode = malloc(sizeof(subscriptionNode));
	newNode->topicSize = currentTopicSize;
	newNode->topic = malloc(currentTopicSize);
	newNode->processPipeID = workerPipeNumber;
	read(routerPipeFd, newNode->topic, currentTopicSize);

	/* get to end of subscriptions linked list, and add new node */
	currentIndexNode = startingNode;
	if (startingNode == NULL) {
		return newNode;
	}
	else {
		while (currentIndexNode->nextNode != NULL) {
			currentIndexNode = currentIndexNode->nextNode;
		}
		currentIndexNode->nextNode = newNode;
	}
	return NULL;
}

void ROUTER_handlePublish (subscriptionNode* startingNode, int routerPipeFd) {
	uint32_t messageSize;
	uint16_t currentTopicSize;
	char* currentPublishTopic;
	char* currentPublishMessage;
	subscriptionNode* currentIndexNode = NULL;

	/* read topic size and message size from WORKER process */
	read(routerPipeFd, &currentTopicSize, sizeof(currentTopicSize));
	read(routerPipeFd, &messageSize, sizeof(messageSize));

	/* read message and topic by WORKER process */
	currentPublishTopic = malloc(currentTopicSize);
	read(routerPipeFd, currentPublishTopic, currentTopicSize);
	currentPublishMessage = malloc(messageSize);
	read(routerPipeFd, currentPublishMessage, messageSize);

	currentIndexNode = startingNode;

	/* if no client is subscribed, drop request */
	if (currentIndexNode == NULL) {
		free(currentPublishTopic);
		free(currentPublishMessage);
		return;
	}

	/* otherwise, search for subscriptions, and notify clients */
	else {
		while (currentIndexNode != NULL) {
			if (currentIndexNode->topicSize == currentTopicSize &&
				equalTopics(currentIndexNode->topic, currentPublishTopic, currentTopicSize)) {
				notifyClient(currentIndexNode, currentPublishMessage, messageSize);
			}
			currentIndexNode = currentIndexNode->nextNode;
		}
	}
}

/* ROUTER listens for messages from other processes and processes pub and sub
requests. It listens to the ROUTER pipe */
void ROUTER () {
	char connectionType;
	int pipeFd;
	subscriptionNode* startingNode = NULL;

	/* create router pipe */
	if (mkfifo(ROUTER_FILE,0644) == -1) {
		perror("Pipe creation error!\n");
	}

	while (1) {
		/* open pipe for reading */
		pipeFd = open(ROUTER_FILE,O_RDONLY);

		read(pipeFd, &connectionType, sizeof(connectionType));

		if (connectionType == 'S') {
			puts("Entering ROUTER_handleSubscribe");
			subscriptionNode* newNode = ROUTER_handleSubscribe(startingNode, pipeFd);
			if (newNode != NULL) startingNode = newNode;
			puts("Exiting ROUTER_handleSubscribe");
		}
		else if (connectionType == 'P') {
			puts("Entering ROUTER_handlePublish");
			ROUTER_handlePublish(startingNode, pipeFd);
			puts("Exiting ROUTER_handlePublish");
		}

		/* request is correctly answered, so close pipe, remove lock and
		wait for next worker process request; */
		close(pipeFd);
		remove(ROUTER_LOCK);
	}
}

packet receivePacket (int connectionSocket) {
	uint8_t currentByte;
	uint8_t messageLenBytes[4];
	int index = 0;

	packet returnedPacket;
	/* read packet connection byte, and store it */
	if (read(connectionSocket, &currentByte, sizeof(currentByte)) == -1) {
		returnedPacket.type = 0x00;
		returnedPacket.contents = malloc(sizeof(uint8_t));
		return returnedPacket;
	}
	returnedPacket.type = currentByte;

	/* receive message length */
	index = 0;
	currentByte = 255;
	while (currentByte >= 127) {
		read(connectionSocket, &currentByte, sizeof(currentByte));
		messageLenBytes[index] = currentByte;
		index++;
	}
	int messageLenLength = index;

	if (returnedPacket.type == 0xC0) {
		puts("PING");
		returnedPacket.contents = malloc(1);
		return returnedPacket;
	}

	/* calculate message length */
	int messageLength = 0;
	for (index = 0; index < messageLenLength; index++) {
		messageLength += (messageLenBytes[index] % 128) << (7*(messageLenLength-index-1));
		index++;
	}

	returnedPacket.length = messageLength;

	/* receive the rest of the packet */
	returnedPacket.contents = malloc(messageLength*sizeof(uint8_t));
	for (index = 0; index < messageLength; index++) {
		read(connectionSocket, &currentByte, sizeof(currentByte));
		returnedPacket.contents[index] = currentByte;
	}

	return returnedPacket;
}

void lockRouter () {
	while (access( ROUTER_LOCK, F_OK ) == 0 ) {
		sleep(0.1);
	}

	FILE* routerLock = fopen(ROUTER_LOCK, "w");
	fclose(routerLock);
	return;
}

int handleClientConnection (int connectionSocket) {
	int index;
	char workerPipeString[100];
	char workerPipeReadyFile[100];
	int routerFd;
	uint8_t router8Buffer[1];
	int workerPipeFd;
	uint16_t topicSize;
	uint32_t messageSize;
	uint32_t packetLength;

	/* read first packet */
	packet currentPacket = receivePacket(connectionSocket);

	/* check is first packet tries to connect */
	if (currentPacket.type != 0x10) {
		puts("First packet didn't ask to connect! Dropping connection!");
		free(currentPacket.contents);
		return 0;
	}

	/* verify if client sends protocol name correctly */
	int correctProtocolNameBytes[7] = {0, 4, 77, 81, 84, 84, 4};
	for (index = 0; index < 7; index++) {
		if (currentPacket.contents[index] != correctProtocolNameBytes[index]) {
			puts("Client didn't send correct protocol name! Dropping connection!");
			return(0);
		}
	}

	/* client connected! can discard current packet and confirm connection! */
	free(currentPacket.contents);
	char response[5];
	response[0] = 0x20;
	response[1] = 0x02;
	response[2] = 0x00;
	response[3] = 0x00;
	write(connectionSocket, response, 4);

	/* create worker process pipe */
	index = 0;
	sprintf(workerPipeString, "/tmp/temp.mac0352.WORKER.%d", index);
	while (access( workerPipeString, F_OK ) == 0) {
		index++;
		sprintf(workerPipeString, "/tmp/temp.mac0352.WORKER.%d", index);
	}
	if (mkfifo(workerPipeString,0644) == -1) {
		perror("Pipe creation error!\n");
	}
	uint16_t clientID = index;

	/* define file name to check for router updates */
	sprintf(workerPipeReadyFile, "/tmp/temp.mac0352.WORKER.%d.ready", index);

	/* put client socket in non-blocking mode */
	fcntl(connectionSocket, F_SETFL, fcntl(connectionSocket, F_GETFL, 0) | O_NONBLOCK);

	/* initiate main client handling loop */
	while (1) {

		/* --------------------- handle client requests --------------------- */
		packet currentPacket = receivePacket(connectionSocket);

		if (currentPacket.type == 0xC0) {
			response[0] = 0xD0;
			response[1] = 0x00;
			write(connectionSocket, response, 2);
			free(currentPacket.contents);
			continue;
		}

		/* handle subscribe requst */
		if (currentPacket.type == 0x82) {
			lockRouter();
			routerFd = open(ROUTER_FILE, O_WRONLY);
			router8Buffer[0] = 'S';
			write(routerFd, router8Buffer, sizeof(uint8_t));

			/*send topic size and client ID */
			topicSize = (currentPacket.contents[2] << 8) + currentPacket.contents[3];

			write(routerFd, &topicSize, sizeof(topicSize));
			write(routerFd, &clientID, sizeof(clientID));

			/* send topic */
			for (index = 4; currentPacket.contents[index] != 0; index++) {
				router8Buffer[0] = currentPacket.contents[index];
				write(routerFd, router8Buffer, sizeof(uint8_t));
			}

			/* send subscribe awknolegement */
			response[0] = 0x90;response[1] = 0x03;
			response[2] = currentPacket.contents[0];
			response[3] = currentPacket.contents[1];
			response[4] = 0x00;
			write(connectionSocket, response, 5);

			close(routerFd);
		}

		else if (currentPacket.type == 0x30) {
			lockRouter();
			routerFd = open(ROUTER_FILE, O_WRONLY);
			router8Buffer[0] = 'P';
			write(routerFd, router8Buffer, sizeof(uint8_t));

			/*send topic size and message size */
			topicSize = (currentPacket.contents[0] << 8) + currentPacket.contents[1];
			messageSize = (currentPacket.length - topicSize - 2);

			write(routerFd, &topicSize, sizeof(topicSize));
			write(routerFd, &messageSize, sizeof(messageSize));

			/* send topic */
			for (index = 2; index < topicSize+2; index++) {
				router8Buffer[0] = currentPacket.contents[index];
				write(routerFd, router8Buffer, sizeof(uint8_t));
			}

			/* send message */
			for (index = 2 + topicSize; index < currentPacket.length; index++) {
				router8Buffer[0] = currentPacket.contents[index];
				write(routerFd, router8Buffer, sizeof(uint8_t));
			}

			close(routerFd);
		}

		else if (currentPacket.type == 0xE0) {
			remove(workerPipeString);
			exit(0);
		}

		free(currentPacket.contents);

		/* --------------------- handle router requests --------------------- */

		if ( access( workerPipeReadyFile, F_OK ) == 0 ) {
			workerPipeFd = open(workerPipeString, O_RDONLY);
			read(workerPipeFd, router8Buffer, sizeof(uint8_t));

			read(workerPipeFd, &topicSize, sizeof(topicSize));
			read(workerPipeFd, &messageSize, sizeof(messageSize));

			packetLength = topicSize + messageSize + 2;

			router8Buffer[0] = 0x30;
			write(connectionSocket, router8Buffer, sizeof(uint8_t));

			router8Buffer[0] = (uint8_t) packetLength;
			write(connectionSocket, router8Buffer, sizeof(uint8_t));

			router8Buffer[0] = (uint8_t) topicSize >> 8;
			write(connectionSocket, router8Buffer, sizeof(uint8_t));
			router8Buffer[0] = (uint8_t) topicSize;
			write(connectionSocket, router8Buffer, sizeof(uint8_t));

			for (index = 0; index < topicSize; index++) {
				read(workerPipeFd, router8Buffer, sizeof(uint8_t));
				write(connectionSocket, router8Buffer, sizeof(uint8_t));
			}

			for (index = 0; index < messageSize; index++) {
				read(workerPipeFd, router8Buffer, sizeof(uint8_t));
				write(connectionSocket, router8Buffer, sizeof(uint8_t));
			}

			close(workerPipeFd);
			remove(workerPipeReadyFile);
		}
	}

	return 0;
}

int main (int argc, char **argv) {

	/* ---------------- Initialize and define variables ---------------- */

	int linstenSocket, connectionSocket;
	struct sockaddr_in serverDetails;
	pid_t childpid;
	int handleReturn = 0;

	/* -------------------- Create and bind to socket -------------------- */

	/* check program arguments */
	if (argc != 2) {
		fprintf(stderr,"Uso: %s <Porta>\n",argv[0]);
		fprintf(stderr,"Vai rodar um servidor broker de MQTT na porta <Porta> TCP\n");
		exit(1);
	}

	/* create socket */
	if ((linstenSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("Socket creation error!\n");
		exit(2);
	}

	/* add socket information */
	bzero(&serverDetails, sizeof(serverDetails));
	serverDetails.sin_family      = AF_INET;
	serverDetails.sin_addr.s_addr = htonl(INADDR_ANY);
	serverDetails.sin_port        = htons(atoi(argv[1]));

	/* bind to socket */
	if (bind(linstenSocket, (struct sockaddr *)&serverDetails, sizeof(serverDetails)) == -1) {
		perror("Bind error! Check if another process is bound to port!\n");
		exit(3);
	}

	/* initialize socket */
	if (listen(linstenSocket, LISTENQ) == -1) {
		perror("Error listening to socket!\n");
		exit(4);
	}

	/* --------------------- Fork to initiate ROUTER --------------------- */

	childpid = fork();

	if (childpid == 0) {
		ROUTER();
		exit(0);
	}


	/* --------------------- Program main loop start --------------------- */

	printf("  -> Servidor no ar! Aguardando conexões na porta %s...\n",argv[1]);

	while (1) {
		/* check for new connection */
		connectionSocket = accept(linstenSocket, (struct sockaddr *) NULL, NULL);
		if ( connectionSocket == -1 ) {
			perror("Error accepting new connection!\n");
			exit(5);
		}

		puts("New connection! Forking!");
		/* fork to handle new connection */
		childpid = fork();

		if ( childpid == 0 ) {
			/* if child, handle connection, and close sockets */
			close(linstenSocket);
			handleReturn = handleClientConnection(connectionSocket);
			close(connectionSocket);
			exit(handleReturn);
		}
		else {
			/* if parent, close sockets and listen for new connections */
			close(connectionSocket);
		}
	}
	exit(0);
}
