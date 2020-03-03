#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <iostream>
#include <thread>
/* You will to add includes here */


uint16_t sockFD;

void sendMsg(void)
{
	char msg[500];
	while (true)
	{
		memset(&msg, 0, sizeof(msg));
		uint16_t numBytes;
		gets(msg);
		msg[strlen(msg)] = '\n';
		if (strlen(msg) <= 256)
		{
		numBytes = send(sockFD, msg, strlen(msg), 0);
		}
		else
		{
			printf("Message too long\n");
		}

	}
}
int main(int argc, char* argv[]) {

	if (argc < 4)
	{
		printf("To few arguments!\n");
		exit(0);
	}
	struct addrinfo hints, * serverInfo, * p;
	uint16_t numBytes;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char servMsg[100];
	uint16_t servMsg_Len = sizeof(servMsg);
	char protocol[20] = "Chat TCP 1.0\n";
	uint16_t returnValue;
	if ((returnValue = getaddrinfo(argv[1], argv[2], &hints, &serverInfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
		exit(0);
	}
	p = serverInfo;
	for (p = serverInfo; p != NULL; p = p->ai_next) {
		if ((sockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}
		if (connect(sockFD, p->ai_addr, p->ai_addrlen) == -1)
		{
			printf("Error connecting: %s\n", strerror(errno));
			close(sockFD);
			exit(1);
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to create socket\n");
		return 2;
	}
	freeaddrinfo(serverInfo);
	numBytes = recv(sockFD, servMsg, servMsg_Len, 0);
	printf("Server protocol: %s", servMsg);
	std::thread sendingThread(sendMsg);
	if (strcmp(servMsg, protocol) == 0)
	{
		printf("Protocol supported, sending nickname\n");
		numBytes = send(sockFD, argv[3], strlen(argv[3]), 0);

	}
	//fcntl(sockFD, F_SETFL, O_NONBLOCK);
	while (true)
	{
		memset(&servMsg, 0, sizeof(servMsg));
		numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0);

		printf("%s", servMsg);

	}
	sendingThread.join();
	close(sockFD);
}

