#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <regex.h>
#include <time.h>
#include <thread>
/* You will to add includes here */




using namespace std;
struct client
{
	char* address;
	uint16_t port;
	uint16_t socket;
	uint16_t score = 0;
	bool inMenu = true;
	bool ready = false;
	uint16_t option = 0;
};
struct game
{
	client* client1;
	client* client2;
	time_t startTime;
	int stage = 0;
	bool gameOver = false;
};
int MAXGAMES = 10;
int nrOfGames = 0;
int MAXCLIENTS = 10;
int nrOfClients = 0;
int MAXTHREADS = 10;
int nrOfThreads = 0;
struct client** clients = new client * [MAXCLIENTS] { nullptr };
struct game** games = new game * [MAXGAMES] { nullptr };
std::thread** threads = new std::thread * [MAXTHREADS] {nullptr};
uint16_t numBytes;
void checkGames(void)
{
	char msgToSend[350];
	char command[20];
	while (true)
	{

		for (int i = 0; i < nrOfGames; i++)
		{
			if (games[i]->gameOver)
			{
				strcpy(command, "MENU");
				sprintf(msgToSend, "%s ", command);
				strcat(msgToSend, "Please select!\n1.Play\n2.Watch\n");
				send(games[i]->client1->socket, msgToSend, strlen(msgToSend), 0);
				send(games[i]->client2->socket, msgToSend, strlen(msgToSend), 0);
				
				delete games[i];
				for (int j = i; j < nrOfGames; j++)
				{
					if (games[j + 1] != nullptr)
					{
						games[j] = games[j + 1];
					}
				}
			}
		}
	}
}
void playGame(game* game)
{
	char msgToSend[100];
	printf("playGame Started\n");
	time_t startTime = time(NULL);
	int counter = 3;
	printf("Start Time: %ld\n", game->startTime);
	bool startSent = false;
	sprintf(msgToSend, "MSG Game will start in %d seconds\n", counter);
	numBytes = send(game->client1->socket, msgToSend, strlen(msgToSend), 0);
	printf("[<]Sent %d bytes\n", numBytes);
	numBytes = send(game->client2->socket, msgToSend, strlen(msgToSend), 0);
	printf("[<]Sent %d bytes\n", numBytes);
	while (!game->gameOver)
	{
		memset(&msgToSend, 0, sizeof(msgToSend));
		if (game != nullptr)
		{

			if (game->client1->score >= 3 || game->client2->score >= 3)
			{
				printf("Game over!\n");
				game->gameOver = true;
			}
			else if (time(NULL) - startTime > 1 && counter > 0)
			{
				sprintf(msgToSend, "MSG Game will start in %d seconds\n", counter);
				numBytes = send(game->client1->socket, msgToSend, strlen(msgToSend), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				numBytes = send(game->client2->socket, msgToSend, strlen(msgToSend), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				startTime = time(NULL);
				counter--;
			}
			else if (!startSent && counter == 0)
			{
				numBytes = send(game->client1->socket, "START \n", strlen("START \n"), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				numBytes = send(game->client2->socket, "START \n", strlen("START \n"), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				startSent = true;
			}
			else if (counter == 0)
			{
				if (game->client1->option > 0 && game->client2->option > 0)
				{
					printf("Both have selected\n");
					if ((game->client1->option == 1 && game->client2->option == 3) ||
						(game->client1->option == 2 && game->client2->option == 1) ||
						(game->client1->option == 3 && game->client2->option == 2))
					{
						printf("Player 1 has won\n");
						send(game->client2->socket, "MSG You've lost\n", strlen("MSG You've lost\n"), 0);
						send(game->client1->socket, "MSG You've won\n", strlen("MSG You've won\n"), 0);
						game->client1->score++;
						game->client1->option = 0;
						game->client2->option = 0;
						startTime = time(NULL);
						counter = 3;
						startSent = false;
					}
					else if ((game->client2->option == 1 && game->client1->option == 3) ||
						(game->client2->option == 2 && game->client1->option == 1) ||
						(game->client2->option == 3 && game->client1->option == 2))
					{
						printf("Player 2 has won\n");
						send(game->client1->socket, "MSG You've lost\n", strlen("MSG You've lost\n"), 0);
						send(game->client2->socket, "MSG You've won\n", strlen("MSG You've won\n"), 0);

						game->client2->score++;

						game->client1->option = 0;
						game->client2->option = 0;
						startTime = time(NULL);
						counter = 3;
						startSent = false;
					}
					else if (game->client1->option == game->client2->option)
					{
						printf("It's a tie\n");
						send(game->client1->socket, "MSG Tie\n", strlen("MSG Tie\n"), 0);
						send(game->client2->socket, "MSG Tie\n", strlen("MSG Tie\n"), 0);
						game->client1->option = 0;
						game->client2->option = 0;
						startTime = time(NULL);
						counter = 3;
						startSent = false;
					}
				}
			}
		}
	}

}
int main(int argc, char* argv[])
{

	if (argc < 2)
	{
		printf("To few arguments!\nExpected <IP(optional)> <port>\n");
		exit(0);
	}

	struct addrinfo guide, * serverInfo, * p;
	struct sockaddr_in theirAddr;
	socklen_t theirAddr_len = sizeof(theirAddr);

	uint8_t returnValue;
	uint16_t sockFD;
	uint16_t tempSFD;

	fd_set master;
	fd_set read_sds;
	int fDMax;

	char clientMsg[350];
	int clientMsgLen = sizeof(clientMsg);
	char msgToSend[350];
	char command[20];
	char buffer[350];
	char protocol[20] = "RPS TCP 1\n";

	struct timeval timeO;
	timeO.tv_sec = 5;
	timeO.tv_usec = 0;

	FD_ZERO(&master);
	FD_ZERO(&read_sds);

	memset(&guide, 0, sizeof(guide));
	guide.ai_family = AF_INET;
	guide.ai_socktype = SOCK_STREAM;
	guide.ai_flags = AI_PASSIVE;

	client* clientQueue[10];
	int clientsInQueue = 0;

	if (argc == 3)
	{
		if ((returnValue = getaddrinfo(argv[1], argv[2], &guide, &serverInfo)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
			exit(0);
		}
	}
	else if (argc == 2)
	{
		if ((returnValue = getaddrinfo(NULL, argv[1], &guide, &serverInfo)) != 0)
		{
			fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
			exit(0);
		}
	}
	for (p = serverInfo; p != NULL; p = p->ai_next) {

		if ((sockFD = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1)
		{
			printf("listener: socket: %s\n", gai_strerror(errno));
			continue;
		}

		if (bind(sockFD, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockFD);
			printf("listener error: bind: %s\n", gai_strerror(errno));
			continue;
		}

		break;
	}
	freeaddrinfo(serverInfo);


	if (listen(sockFD, 1) == -1)
	{
		printf("No-one listens to me:(\nError: %s", strerror(errno));
		close(sockFD);
		exit(0);
	}
	else
	{
		if (argc == 3)
		{
			printf("[x]Listening on %s:%s\n", argv[1], argv[2]);
		}
		else if (argc == 2)
		{
			struct sockaddr_in addres;
			int sa_len;
			sa_len = sizeof(addres);
			getsockname(sockFD, (struct sockaddr*) & addres, &sa_len);

			char servAddr[250];
			socklen_t bl = sizeof(servAddr);
			inet_ntop(addres.sin_family, &addres.sin_addr, servAddr, bl);
			printf("[x]Listening on %s:%s\n", servAddr, argv[1]);
		}
	}
	FD_SET(sockFD, &master);
	fDMax = sockFD;
	std::thread supervisorThread(checkGames);
	while (true)
	{

		memset(&command, 0, sizeof(command));
		memset(&buffer, 0, sizeof(buffer));
		memset(&clientMsg, 0, clientMsgLen);
		memset(&msgToSend, 0, sizeof(msgToSend));
		read_sds = master;
		if (select(fDMax + 1, &read_sds, NULL, NULL, &timeO) == -1)
		{
			printf("Select error: %s\n", strerror(errno));
			close(sockFD);
		}

		for (int i = 0; i <= fDMax; i++)
		{
			if (FD_ISSET(i, &read_sds))
			{
				if (i == sockFD)
				{
					if ((tempSFD = accept(sockFD, (struct sockaddr*) & theirAddr, &theirAddr_len)) == -1)
					{
						printf("Error accepting: %s\n", strerror(errno));
					}
					else
					{
						printf("Client connected from %s:%d\n", inet_ntoa(theirAddr.sin_addr), ntohs(theirAddr.sin_port));
						clients[nrOfClients] = new client;
						clients[nrOfClients]->address = inet_ntoa(theirAddr.sin_addr);
						clients[nrOfClients]->port = ntohs(theirAddr.sin_port);
						clients[nrOfClients]->socket = tempSFD;
						printf("%s", protocol);
						numBytes = send(clients[nrOfClients]->socket, protocol, strlen(protocol), 0);
						printf("[<]Sent %d bytes\n", numBytes);


						FD_SET(clients[nrOfClients]->socket, &master);
						if (clients[nrOfClients]->socket > fDMax)
						{
							fDMax = clients[nrOfClients]->socket;
						}
						nrOfClients++;
					}
				}
				else
				{
					if ((numBytes = recv(i, clientMsg, clientMsgLen, 0)) <= 0)
					{
						if (numBytes == 0)
						{
							bool found = false;
							printf("Client left\n");
							for (int h = 0; h < nrOfClients && !found; h++)
							{
								if (clients[h]->socket == i)
								{
									delete clients[h];
									for (int y = h; y < nrOfClients; y++)
									{
										if (clients[y + 1] != nullptr)
										{

											clients[y] = clients[y + 1];
										}
									}
									nrOfClients--;
									found = true;
								}
							}

							printf("Clients: %d\n", nrOfClients);

							close(i);
						}
						else
						{
							printf("Error recieving: %s\n", strerror(errno));

							close(i);
						}
						FD_CLR(i, &master);
					}
					else
					{
						printf("%s", clientMsg);

						sscanf(clientMsg, "%s %[^\n]", command, buffer);
						struct sockaddr_in addres;
						int sa_len;
						sa_len = sizeof(addres);
						getpeername(i, (struct sockaddr*) & addres, &sa_len);

						char bobp[250];
						socklen_t bl = sizeof(bobp);
						inet_ntop(addres.sin_family, &addres.sin_addr, bobp, bl);
						for (int b = 0; b < nrOfClients; b++)
						{
							if (strcmp(bobp, clients[b]->address) == 0 && ntohs(addres.sin_port) == clients[b]->port)
							{

								if (strcmp(command, "CONACC") == 0)
								{
									strcpy(command, "MENU");
									sprintf(msgToSend, "%s ", command);
									strcat(msgToSend, "Please select!\n1.Play\n2.Watch\n");
									printf("%s", msgToSend);
									numBytes = send(clients[b]->socket, msgToSend, strlen(msgToSend), 0);
									printf("[<]Sent %d bytes\n", numBytes);
								}
								else if (strcmp(command, "OPT") == 0)
								{

									if (clients[b]->inMenu == true)
									{
										if (strcmp(buffer, "1") == 0)
										{
											clients[b]->inMenu = false;
											clientQueue[clientsInQueue] = clients[b];
											clientsInQueue++;
											if (clientsInQueue >= 2)
											{
												numBytes = send(clientQueue[0]->socket, "RDY \n", strlen("RDY \n"), 0);
												printf("[<]Sent %d bytes\n", numBytes);

												numBytes = send(clientQueue[1]->socket, "RDY \n", strlen("RDY \n"), 0);
												printf("[<]Sent %d bytes\n", numBytes);
											}

										}
									}
									else
									{
										for (int u = 0; u < nrOfGames; u++)
										{
											if (games[u]->client1->socket == i)
											{
												printf("Option selected\n");
												if (strcmp(buffer, "1") == 0)
												{
													games[u]->client1->option = 1;

												}
												else if (strcmp(buffer, "2") == 0)
												{
													games[u]->client1->option = 2;

												}
												else if (strcmp(buffer, "3") == 0)
												{
													games[u]->client1->option = 3;

												}

											}
											else if (games[u]->client2->socket == i)
											{
												printf("Option selected\n");
												if (strcmp(buffer, "1") == 0)
												{
													games[u]->client2->option = 1;

												}
												else if (strcmp(buffer, "2") == 0)
												{
													games[u]->client2->option = 2;

												}
												else if (strcmp(buffer, "3") == 0)
												{
													games[u]->client2->option = 3;

												}
											}
										}
									}
								}
								else if (strcmp(command, "RDY") == 0)
								{
									clients[b]->ready = true;
									if (clientsInQueue >= 2)
									{
										bool found = false;
										for (int j = 0; j < clientsInQueue; j++)
										{
											if (clientQueue[j]->ready)
											{
												for (int k = j + 1; k < clientsInQueue && !found; k++)
												{
													if (clientQueue[k]->ready)
													{
														found = true;
														games[nrOfGames] = new game;
														games[nrOfGames]->client1 = clientQueue[j];
														games[nrOfGames]->client2 = clientQueue[k];
														threads = new std::thread(playGame, games[nrOfGames]);
														for (int l = k; l < clientsInQueue; l++)
														{
															clientQueue[l] = clientQueue[l + 1];
														}
														clientsInQueue--;
														for (int l = j; l < clientsInQueue; l++)
														{
															clientQueue[l] = clientQueue[l + 1];
														}
														clientsInQueue--;
														nrOfGames++;
													}
												}
											}
										}
										printf("Game start\n");
									}
								}
								else
								{
									send(clients[b]->socket, "ERROR Unkown command\n", strlen("ERROR Unkown command\n"), 0);

								}
							}
						}
					}

				}
			}
		}
	}
	for (int i = 0; i < nrOfClients; i++)
	{
		close(clients[i]->socket);
		delete clients[i];
	}
	delete[] clients;
	return 0;
}
