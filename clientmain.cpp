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

uint16_t sockFD;
int main(int argc, char *argv[])
{

	if (argc < 3)
	{
		printf("To few arguments!\nExpected: <server-ip> <server-port>");
		exit(0);
	}

	uint16_t returnValue, numBytes;

	struct addrinfo hints, *serverInfo, *p;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	char servMsg[500], msgToPrint[500], command[40], input[500], msgToSend[550];

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
	int FDMax = fileno(stdin);
	int stdinSock = fileno(stdin);
	bool spectating = false;
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
		if (numBytes < 1)
		{
			std::cout << "lost Connection\n";
			break;
		}
		else
		{
			if (FD_ISSET(stdinSock, &read_sds))
			{
				if (spectating)
				{
					send(sockFD, "OPTION -1\n", strlen("OPTION -1\n"), 0);
					spectating = false;
				}
				else
				{
					read(stdinSock, input, sizeof(input));
					sprintf(msgToSend, "OPTION %s", input);
					numBytes = send(sockFD, msgToSend, strlen(msgToSend), 0);
				}
				FD_CLR(stdinSock, &read_sds);
			}
			if (FD_ISSET(sockFD, &read_sds))
			{
				if ((numBytes = recv(sockFD, servMsg, sizeof(servMsg), 0)) < 0)
				{
					break;
				}
				std::string msgTemp = servMsg;
				while (strlen(servMsg) > strlen(command) + strlen(msgToPrint))
				{

					sscanf(servMsg, "%s %[^\n]", command, msgToPrint);

					if (strcmp(command, "MENU") == 0)
					{
						printf("Please select:\n1. Play\n2. Watch\n3. Highscores\n0. Exit\n");
					}
					else if (strcmp(command, "READY") == 0)
					{
						system("clear");
						printf("Game is ready\n");
						numBytes = send(sockFD, "READY \n", strlen("READY \n"), 0);
					}
					else if (strcmp(command, "STRT") == 0)
					{
						printf("Please select an option\n1.Rock\n2.Paper\n3.Scissor\n");
					}
					else if (strcmp(command, "OVER") == 0)
					{
						if (spectating)
						{
							numBytes = send(sockFD, "OPTION 2\n", strlen("OPTION 2\n"), 0);
							spectating = false;
						}
						else
						{
							numBytes = send(sockFD, "RESET \n", strlen("RESET \n"), 0);
						}

						printf("Game Over\n");
					}
					else if (strcmp(command, "SPEC") == 0)
					{
						bool stop = false;
						int nrOfGames = atoi(msgToPrint);
						if (nrOfGames > 0)
						{
							std::cout << "Please type the number of the game you want to spectate\nAvailable games: \n";

							for (int i = 0; i < nrOfGames && !stop; ++i)
							{
								msgTemp.erase(0, strlen(command) + strlen(msgToPrint) + 2);
								strcpy(servMsg, msgTemp.c_str());
								sscanf(servMsg, "%s %[^\n]", command, msgToPrint);
								if (strcmp(command, "SPEC") == 0)
								{
									std::cout << msgToPrint << std::endl;
								}
								else
								{
									stop = true;
								}
							}
						}
						else
						{
							std::cout << "No games available, returning to menu\n";
							send(sockFD, "RESET \n", strlen("RESET \n"), 0);
						}
					}
					else if (strcmp(command, "SPECACC") == 0)
					{
						system("clear");
						std::cout << "You are now spectating a game, press \"Enter\" the spectator menu\n";
						spectating = true;
					}
					else if (strcmp(command, "SPECREJ") == 0)
					{
						std::cout << "Unable to spectate, game doesn't exist\n";

						send(sockFD, "RESET \n", strlen("RESET \n"), 0);
					}
					else if (strcmp(command, "SCORE") == 0)
					{
						bool stop = false;
						memset(&input, 0, sizeof(input));
						int nrOfScores = atoi(msgToPrint);
						system("clear");
						std::cout << "These are the average response times, sorted lowest to highest\n";
						if (nrOfScores > 0)
						{

							for (int i = 0; i < nrOfScores && !stop; i++)
							{
								msgTemp.erase(0, strlen(command) + strlen(msgToPrint) + 2);
								strcpy(servMsg, msgTemp.c_str());
								sscanf(servMsg, "%s %[^\n]", command, msgToPrint);
								if (strcmp(command, "SCORE") == 0)
								{
									std::cout << (i + 1) << ": " << msgToPrint << std::endl;
								}
								else
								{
									stop = true;
								}
							}
						}
						else
						{
							std::cout << "No scores available!\n";
						}
							send(sockFD, "RESET \n", strlen("RESET \n"), 0);
					}
					else if (strcmp(command, "MESG") == 0)
					{
						printf("%s\n", msgToPrint);
					}
					else if (strcmp(command, "CLEARCON") == 0)
					{
						system("clear");
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
