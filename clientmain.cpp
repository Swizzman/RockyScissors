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
#include <regex.h>
/* You will to add includes here */


uint16_t sockFD;

void sendMsg(void)
{
	char input[500];
	char msgToSend[270];
	while (true)
	{
		memset(&msgToSend, 0, sizeof(msgToSend));
		memset(&input, 0, sizeof(input));
		uint16_t numBytes;
		fgets(input, sizeof(input), stdin);
		if (strstr(input, "NICK ") != NULL)
		{
			if (strlen(input) < 17)
			{

				numBytes = send(sockFD, input, strlen(input), 0);
			}
			else
			{
				printf("Name too long\n");
			}


		}
		else
		{
			if (strlen(input)  <1)
			{
				printf("Message too short\n");
			}
			else if (strlen(input) <= 250 )
			{
				sprintf(msgToSend, "MSG %s\n", input);
				numBytes = send(sockFD, msgToSend, strlen(msgToSend), 0);
			}
			else
			{
				printf("Message too long\n");
			}
		}

	}
}
int main(int argc, char* argv[]) {

	if (argc < 4)
	{
		printf("To few arguments!\nExpected: <server-ip> <server-port> <nickname>");
		exit(0);
	}
	if (strlen(argv[3]) > 12)
	{
		printf("Error, name too long");
		exit(0);
	}
	regex_t regex;
	uint16_t returnValue;
	returnValue = regcomp(&regex, "^[_[:alnum:]]*$", 0);
	if (returnValue != 0)
	{
		printf("Error compiling regex\n");
		exit(0);
	}
	returnValue = regexec(&regex, argv[3], 0, NULL, 0);
	if (returnValue == REG_NOMATCH)
	{
		printf("Name does not match\n");
		exit(0);
	}
	struct addrinfo hints, * serverInfo, * p;
	uint16_t numBytes;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char servMsg[270];
	int servMsg_Len = sizeof(servMsg);
	char msgToPrint[270];
	memset(&servMsg, 0, sizeof(servMsg));
	char protocol[20] = "HELLO 1\n";
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
	struct sockaddr_in addres;
	int sa_len;
	sa_len = sizeof(addres);
	getpeername(sockFD, (struct sockaddr*) & addres, &sa_len);

	char servAddr[250];
	socklen_t bl = sizeof(servAddr);
	inet_ntop(addres.sin_family, &addres.sin_addr, servAddr, bl);
	printf("Connected to %s:%d\n", servAddr, ntohs(addres.sin_port));
	freeaddrinfo(serverInfo);
	numBytes = recv(sockFD, servMsg, servMsg_Len, 0);

	printf("Server protocol: %s", servMsg);
	if (strcmp(servMsg, protocol) == 0)
	{
		printf("Protocol supported, sending nickname\n");
		sprintf(servMsg, "NICK %s", argv[3]);
		numBytes = send(sockFD, servMsg, strlen(servMsg), 0);
		memset(&servMsg, 0, sizeof(servMsg));
		numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0);
		if (strcmp(servMsg, "OK\n") != 0)
		{
			printf("Name not accepted\n");
			close(sockFD);
			exit(0);
		}
		else
		{
			printf("Name accepted!\n");
		}

	}
	else
	{
		printf("Protocol not supported\n");
		close(sockFD);
		exit(0);
	}
	std::thread sendingThread(sendMsg);
	while (true)
	{
		char command[4];
		memset(&command, 0, sizeof(command));
		memset(&servMsg, 0, sizeof(servMsg));
		memset(&msgToPrint, 0, sizeof(msgToPrint));
		numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0);

		if (numBytes > 0)
		{
			sscanf(servMsg, "%s", command);
			//printf("[>]Recieved %d bytes\n", numBytes);
			if (strcmp(command, "ERROR") == 0)
			{
				sscanf(servMsg, "%s %[^\n]", command, msgToPrint);
				printf("Error: %s\n", msgToPrint);
			}
			else if (strcmp(command, "OK") == 0)
			{
				printf("Name accepted!\n");
			}
			else
			{
				char name[20];
				memset(&name, 0, sizeof(name));
				sscanf(servMsg, "%s %s %[^\n]", command, name, msgToPrint);
				printf("%s: %s\n", name, msgToPrint);

			}
		}
		else
		{
			break;
		}



	}
	sendingThread.join();
	close(sockFD);
}

