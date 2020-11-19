#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
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

	memset(&msgToSend, 0, sizeof(msgToSend));
	memset(&input, 0, sizeof(input));
	fgets(input, sizeof(input), stdin);
}
int main(int argc, char *argv[])
{

	if (argc < 3)
	{
		printf("To few arguments!\nExpected: <server-ip> <server-port>");
		exit(0);
	}

	uint16_t returnValue;

	struct addrinfo hints, *serverInfo, *p;
	uint16_t numBytes;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char servMsg[500];
	int servMsg_Len = sizeof(servMsg);
	char msgToPrint[500];
	char command[40];
	char input[500];
	char msgToSend[550];
	memset(&servMsg, 0, sizeof(servMsg));
	char protocol[20] = "RPS TCP 1\n";
	if ((returnValue = getaddrinfo(argv[1], argv[2], &hints, &serverInfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
		exit(0);
	}
	p = serverInfo;
	for (p = serverInfo; p != NULL; p = p->ai_next)
	{
		if ((sockFD = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
		{
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
	if (p == NULL)
	{
		fprintf(stderr, "client: failed to create socket\n");
		return 2;
	}
	int flag = 1;
	setsockopt(sockFD, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
	struct sockaddr_in addres;
	socklen_t sa_len;
	sa_len = sizeof(addres);
	getpeername(sockFD, (struct sockaddr *)&addres, &sa_len);

	char servAddr[250];
	socklen_t bl = sizeof(servAddr);
	inet_ntop(addres.sin_family, &addres.sin_addr, servAddr, bl);
	printf("Connected to %s:%d\n", servAddr, ntohs(addres.sin_port));
	freeaddrinfo(serverInfo);
	numBytes = recv(sockFD, servMsg, servMsg_Len, 0);

	printf("Server protocol: %s", servMsg);
	if (strcmp(servMsg, protocol) == 0)
	{
		printf("Protocol supported\n");
		numBytes = send(sockFD, "CONA \n", strlen("CONA \n"), 0);
		printf("[<]Sent %d bytes\n", numBytes);
	}
	else
	{
		printf("Protocol not supported\n");
		close(sockFD);
		exit(0);
	}
	int itr = 0;
	while (true)
	{
		memset(&msgToSend, 0, sizeof(msgToSend));
		memset(&input, 0, sizeof(input));
		memset(&servMsg, 0, sizeof(servMsg));

		memset(&command, 0, sizeof(command));
		memset(&msgToPrint, 0, sizeof(msgToPrint));
		if ((numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0)) < 0)
		{
			break;
		}
		// std::cout << servMsg << std::endl;
		std::string msgTemp = servMsg;
		while (strlen(servMsg) > strlen(msgToPrint) + strlen(command))
		{
			// printf("[<]Recieved %d bytes\n", numBytes);
			// printf("%s\n", servMsg);
			std::cout << "Message nr: " << itr << std::endl;
			sscanf(servMsg, "%s %[^\n]", command, msgToPrint);
			// std::cout << "ServMsgSize: " << strlen(servMsg) << std::endl;
			// std::cout << "Scanned Size: " << strlen(msgToPrint) + strlen(command) + 2 << std::endl;
			// std::cout << command << msgToPrint << std::endl;
			if (strcmp(command, "MENU") == 0)
			{
				printf("Please select:\n1. Play\n2. Watch\n");
				fgets(input, sizeof(input), stdin);
				if ((strcmp(input, "1\n") == 0) || (strcmp(input, "2\n") == 0))
				{
					sprintf(msgToSend, "OPTN %s", input);
					numBytes = send(sockFD, msgToSend, strlen(msgToSend), 0);
					printf("[<]Sent %d bytes\n", numBytes);
				}
				else
				{
					printf("Wrong input!\n");
				}
			}
			else if (strcmp(command, "REDY") == 0)
			{
				printf("Game is ready\n");
				numBytes = send(sockFD, "REDY \n", strlen("REDY \n"), 0);
				printf("[<]Sent %d bytes\n", numBytes);
			}
			else if (strcmp(command, "STRT") == 0)
			{
				printf("Please select an option\n1.Rock\n2.Paper\n3.Scissor\n");
				fgets(input, sizeof(input), stdin);
				if (strchr(input, '1') != NULL)
				{
					printf("You have selected Rock!\n");
					numBytes = send(sockFD, "OPTN 1\n", strlen("OPTN 1\n"), 0);
					printf("[<]Sent %d bytes\n", numBytes);
				}
				else if (strchr(input, '2') != NULL)
				{
					printf("You have selected Paper!\n");
					numBytes = send(sockFD, "OPTN 2\n", strlen("OPTN 2\n"), 0);
					printf("[<]Sent %d bytes\n", numBytes);
				}
				else if (strchr(input, '3') != NULL)
				{
					printf("You have selected Scissor!\n");
					numBytes = send(sockFD, "OPTN 3\n", strlen("OPTN 3\n"), 0);
					printf("[<]Sent %d bytes\n", numBytes);
				}
			}
			else if (strcmp(command, "OVER") == 0)
			{
				printf("Game Over\n");
				numBytes = send(sockFD, "RSET \n", strlen("RSET \n"), 0);
			}
			else if (strcmp(command, "SPEC") == 0)
			{
				memset(&input, 0, sizeof(input));
				printf("%s\n", msgToPrint);
				fgets(input, sizeof(input), stdin);
				sprintf(msgToSend, "OPTN %s", input);
				numBytes = send(sockFD, msgToSend, strlen(msgToSend), 0);
				printf("[<]Sent %d bytes\n", numBytes);
			}

			else if (strcmp(command, "MESG") == 0)
			{
				printf("%s\n", msgToPrint);
			}
			itr++;

			msgTemp.erase(0, strlen(command) + strlen(msgToPrint) + 2);
			strcpy(servMsg, msgTemp.c_str());

			memset(&command, 0, sizeof(command));
			memset(&msgToPrint, 0, sizeof(msgToPrint));
		}
		itr = 0;
	}
	close(sockFD);
}
