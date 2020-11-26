#include <stdio.h>
#include <stdlib.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/select.h>
#include <netdb.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <fcntl.h>
#include <iostream>
#define BUFFERSIZE 512
#define INPUTSIZE 256
#define PROTOCOLSIZE 20
int main(int argc, char *argv[])
{

	if (argc < 3)
	{
		printf("To few arguments!\nExpected: <server-ip> <server-port>");
		exit(0);
	}

	uint16_t returnValue, numBytes, sockFD;

	struct addrinfo hints, *serverInfo, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char servMsg[BUFFERSIZE], msgToPrint[BUFFERSIZE], command[BUFFERSIZE], input[INPUTSIZE], msgToSend[BUFFERSIZE];

	memset(&servMsg, 0, sizeof(servMsg));
	char protocol[PROTOCOLSIZE] = "RPS TCP 1\n";
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
	numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0);

	printf("Server protocol: %s", servMsg);
	if (strcmp(servMsg, protocol) == 0)
	{
		printf("Protocol supported\n");
		numBytes = send(sockFD, "CONACC \n", strlen("CONACC \n"), 0);
		printf("[<]Sent %d bytes\n", numBytes);
	}
	else
	{
		printf("Protocol not supported\n");
		close(sockFD);
		exit(0);
	}

	fd_set master;
	fd_set read_sds;

	FD_ZERO(&master);
	FD_ZERO(&read_sds);
	FD_SET(sockFD, &master);
	FD_SET(fileno(stdin), &master);
	int FDMax = fileno(stdin), stdinSock = fileno(stdin);
	bool spectatorMode = false, scoreMode = false;
	int scoreItr = 0, specItr = 0;
	while (true)
	{
		if (sockFD > FDMax)
		{
			FDMax = sockFD;
		}

		memset(&msgToSend, 0, sizeof(msgToSend));
		memset(&input, 0, sizeof(input));
		memset(&servMsg, 0, sizeof(servMsg));

		memset(&command, 0, sizeof(command));
		memset(&msgToPrint, 0, sizeof(msgToPrint));
		read_sds = master;
		numBytes = select(FDMax + 1, &read_sds, NULL, NULL, NULL);
		//Using a c++ string to get access to the erase function later
		std::string msgTemp;
		if (numBytes < 1)
		{
			std::cout << "lost Connection\n";
			break;
		}
		else
		{
			if (FD_ISSET(stdinSock, &read_sds))
			{

				read(stdinSock, input, sizeof(input));
				sprintf(msgToSend, "OPTION %s\n", input);
				numBytes = send(sockFD, msgToSend, strlen(msgToSend), 0);
				fflush(stdin);
					FD_CLR(stdinSock, &read_sds);
			}
			if (FD_ISSET(sockFD, &read_sds))
			{
				if ((numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0)) <= 0)
				{
					std::cout << "Lost Connection to server\n";
					break;
				}
				msgTemp = servMsg;
				while (strlen(servMsg) > 0)
				{
					//Read the command and eventual message
					sscanf(servMsg, "%s %[^\n]", command, msgToPrint);

					if (strcmp(command, "MENU") == 0)
					{
						printf("Please select:\n1. Play\n2. Watch\n3. Highscores\n0. Exit\n");
					}
					else if (strcmp(command, "READY") == 0)
					{
						system("clear");
						std::cout << "A game is ready, press \"Enter\" to accept\n";
					}
					else if (strcmp(command, "STRT") == 0)
					{
						printf("Please select an option\n1.Rock\n2.Paper\n3.Scissor\n");
					}
					else if (strcmp(command, "OVER") == 0)
					{
						system("clear");
						numBytes = send(sockFD, "RESET 1\n", strlen("RESET 1\n"), 0);
						std::cout << "Game over\n";
					}
					else if (strcmp(command, "SPEC") == 0)
					{
						if (!spectatorMode)
						{
							spectatorMode = true;
							std::cout << "Please type the number of the game you want to spectate\nAvailable games: \n";
						}
						if (strcmp(msgToPrint, "NONE") != 0)
						{
							specItr++;
							std::cout << msgToPrint << std::endl;
						}
					}
					else if (strcmp(command, "SPECDONE") == 0)
					{
						spectatorMode = false;
						if (specItr <= 0)
						{
							std::cout << "No games available, returning to menu\n";
							send(sockFD, "RESET \n", strlen("RESET \n"), 0);
						}
						specItr = 0;
					}
					else if (strcmp(command, "SPECACC") == 0)
					{
						system("clear");
						std::cout << "You are now spectating a game, press \"Enter\" to go back to the spectator menu\n";
					}
					else if (strcmp(command, "SPECREJ") == 0)
					{
						std::cout << "Unable to spectate, game doesn't exist\nReturning to menu";

						send(sockFD, "RESET \n", strlen("RESET \n"), 0);
					}
					else if (strcmp(command, "SCORE") == 0)
					{
						if (!scoreMode)
						{
							scoreMode = true;
							system("clear");
							std::cout << "These are the average response times, sorted lowest to highest\n";
						}
						if (strcmp(msgToPrint, "NONE") != 0)
						{
							scoreItr++;
							std::cout << scoreItr << ": " << msgToPrint << std::endl;
						}
					}
					else if (strcmp(command, "SCOREDONE") == 0)
					{
						if (scoreItr <= 0)
						{
							system("clear");
							std::cout << "No scores available!\n";
						}
						scoreMode = false;
						scoreItr = 0;
						send(sockFD, "RESET \n", strlen("RESET \n"), 0);
					}
					else if (strcmp(command, "MESG") == 0)
					{
						printf("%s\n", msgToPrint);
					}
					else if (strcmp(command, "QUIT") == 0)
					{
						close(sockFD);
						exit(0);
					}
					else if (strcmp(command, "EROR") == 0)
					{
						std::cout << "ERROR: " << msgToPrint << std::endl;
					}

					msgTemp.erase(0, strlen(command) + strlen(msgToPrint) + 2);
					strcpy(servMsg, msgTemp.c_str());
					memset(&command, 0, sizeof(command));
					memset(&msgToPrint, 0, sizeof(msgToPrint));
				}
				FD_CLR(sockFD, &read_sds);
			}
		}
	}
	close(sockFD);
}