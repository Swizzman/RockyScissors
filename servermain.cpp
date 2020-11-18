#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <iostream>
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <regex.h>
#include <time.h>
#include <thread>
#include <pthread.h>
#define ROCK 1
#define PAPER 2
#define SCISSORS 3

/* You will to add includes here */
#define MAX_SPECTATORS 10
#define MAX_GAMES 10
#define MAX_CLIENTS 100
#define MAX_THREADS 10
struct client
{
	char *address;
	uint16_t port;
	uint16_t socket;
	uint16_t score = 0;
	bool inMenu = true;
	bool ready = false;
	bool isSpectator = false;
	uint16_t option = 0;
};
struct game
{
	client *player1;
	client *player2;
	client *spectators[MAX_SPECTATORS];
	time_t startTime;
	int nrOfSpectators = 0;
	bool gameOver = false;
};
int nrOfGames = 0;
int nrOfClients = 0;
int nrOfThreads = 0;
struct client **clients = new client *[MAX_CLIENTS] { nullptr };
struct game **games = new game *[MAX_GAMES] { nullptr };
std::thread **threads = new std::thread *[MAX_THREADS] { nullptr };
uint16_t numBytes;
void checkGames(void)
{

	while (true)
	{

		for (int i = 0; i < nrOfGames; i++)
		{
			if (games[i]->gameOver)
			{
				threads[i]->join();
				games[i]->player1->option = 0;
				games[i]->player1->ready = false;
				games[i]->player1->score = 0;

				games[i]->player2->option = 0;
				games[i]->player2->ready = false;
				games[i]->player2->score = 0;
				numBytes = send(games[i]->player1->socket, "OVER \0", strlen("OVER \0"), 0);
				numBytes = send(games[i]->player2->socket, "OVER \0", strlen("OVER \0"), 0);
				for (int k = 0; k < games[i]->nrOfSpectators; ++k)
				{
					numBytes = send(games[i]->spectators[k]->socket, "OVER \0", strlen("OVER \0"), 0);
					games[i]->spectators[k]->isSpectator = false;
					games[i]->spectators[k]->ready = false;
					games[i]->spectators[k]->score = 0;
				}
				delete games[i];
				delete threads[i];
				for (int j = i; j < nrOfGames; j++)
				{
					if (games[j + 1] != nullptr)
					{
						printf("Game is over, deleting\n");
						games[j] = games[j + 1];
						threads[j] = threads[j + 1];
					}
				}
				nrOfGames--;
				nrOfThreads--;
			}
		}
	}
}
void playGame(game *game)
{
	char specMsg[100];
	char player1Msg[100];
	char player2Msg[100];
	time_t startTime = time(NULL) + 1;
	int counter = 4;
	printf("Start Time: %ld\n", game->startTime);
	bool startSent = false;
	while (!game->gameOver)
	{
		memset(&specMsg, 0, sizeof(specMsg));

		if (game != nullptr)
		{

			if (game->player1->score >= 3 || game->player2->score >= 3)
			{
				sleep(0.5);
				printf("Game over!\n");
				game->gameOver = true;
			}
			else if (time(NULL) - startTime > 1 && counter > 0)
			{
				sprintf(specMsg, "MSG Game will start in %d seconds\n", counter - 1);
				numBytes = send(game->player1->socket, specMsg, strlen(specMsg), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				numBytes = send(game->player2->socket, specMsg, strlen(specMsg), 0);

				printf("[<]Sent %d bytes\n", numBytes);
				for (int i = 0; i < game->nrOfSpectators; i++)
				{
					if (game->spectators[i])
					{
						numBytes = send(game->spectators[i]->socket, specMsg, strlen(specMsg), 0);
						printf("[<]Sent %d bytes\n", numBytes);
					}
				}

				startTime = time(NULL);
				counter--;
			}
			else if (!startSent && counter == 0)
			{
				numBytes = send(game->player1->socket, "START \n", strlen("START \n"), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				numBytes = send(game->player2->socket, "START \n", strlen("START \n"), 0);
				printf("[<]Sent %d bytes\n", numBytes);
				startSent = true;
			}
			else if (counter == 0)
			{
				if (game->player1->option > 0 && game->player2->option > 0)
				{
					printf("Both have selected\n");
					switch (game->player1->option)
					{
					case ROCK:
						strcat(specMsg, "MSG Player 1 selected ROCK");
						break;
					case PAPER:
						strcat(specMsg, "MSG Player 1 selected PAPER");
						break;
					case SCISSORS:
						strcat(specMsg, "MSG Player 1 selected SCISSORS");
						break;
					}
					switch (game->player2->option)
					{
					case ROCK:
						strcat(specMsg, ", Player 2 selected ROCK");
						break;
					case PAPER:
						strcat(specMsg, ", Player 2 selected PAPER");
						break;
					case SCISSORS:
						strcat(specMsg, ", Player 2 selected SCISSORS");
						break;
					}
					printf("%s\n", specMsg);
					if ((game->player1->option == ROCK && game->player2->option == SCISSORS) ||
						(game->player1->option == PAPER && game->player2->option == ROCK) ||
						(game->player1->option == SCISSORS && game->player2->option == PAPER))
					{
						printf("Player 1 has won\n");
						game->player1->score++;
						sprintf(player1Msg, "MSG You've won: Score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(player2Msg, "MSG You've lost: Score %d - %d\n", game->player2->score, game->player1->score);

						sprintf(specMsg, "MSG Player 1 has won: Score %d - %d\n", game->player1->score, game->player2->score);
					}
					else if ((game->player2->option == ROCK && game->player1->option == SCISSORS) ||
							 (game->player2->option == PAPER && game->player1->option == ROCK) ||
							 (game->player2->option == SCISSORS && game->player1->option == PAPER))
					{
						printf("Player 2 has won\n");
						game->player2->score++;
						sprintf(player1Msg, "MSG You've lost: Score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(player2Msg, "MSG You've won: Score %d - %d\n", game->player2->score, game->player1->score);
						sprintf(specMsg, "MSG Player 2 has won: Score %d - %d\n", game->player1->score, game->player2->score);
					}
					else
					{
						printf("It's a tie\n");
						sprintf(specMsg, "MSG It's a tie: Score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(player1Msg, "MSG It's a tie: Score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(player2Msg, "MSG It's a tie: Score %d - %d\n", game->player2->score, game->player1->score);
					}
					send(game->player1->socket, player1Msg, strlen(player1Msg), 0);
					send(game->player2->socket, player2Msg, strlen(player2Msg), 0);
					printf("Nr of spectators: %d\n", game->nrOfSpectators);
					for (int i = 0; i < game->nrOfSpectators; ++i)
					{
						if (game->spectators[i])
						{
							numBytes = send(game->spectators[i]->socket, specMsg, strlen(specMsg), 0);
							printf("[<]Sent %d bytes\n", numBytes);
						}
					}

					game->player1->option = 0;
					game->player2->option = 0;
					startTime = time(NULL) + 1;
					counter = 4;
					startSent = false;
				}
			}
		}
	}
}
int main(int argc, char *argv[])
{

	if (argc < 2)
	{
		printf("To few arguments!\nExpected <IP(optional)> <port>\n");
		exit(0);
	}

	struct addrinfo guide, *serverInfo, *p;
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

	client *clientQueue[10];
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
	for (p = serverInfo; p != NULL; p = p->ai_next)
	{

		if ((sockFD = socket(p->ai_family, p->ai_socktype,
							 p->ai_protocol)) == -1)
		{
			printf("listener: socket: %s\n", gai_strerror(errno));
			continue;
		}

		if (bind(sockFD, p->ai_addr, p->ai_addrlen) == -1)
		{
			close(sockFD);
			printf("listener error: bind: %s\n", gai_strerror(errno));
			continue;
		}
		int flag = 1;
		setsockopt(sockFD, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));

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
			socklen_t sa_len;
			sa_len = sizeof(addres);
			getsockname(sockFD, (struct sockaddr *)&addres, &sa_len);

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
					if ((tempSFD = accept(sockFD, (struct sockaddr *)&theirAddr, &theirAddr_len)) == -1)
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

							for (int k = 0; k < clientsInQueue && !found; k++)
							{
								if (clientQueue[k]->socket == i)
								{
									for (int j = k; j < clientsInQueue; j++)
									{
										clientQueue[j] = clientQueue[j + 1];
									}
									found = true;
									clientsInQueue--;
								}
							}

							found = false;
							for (int h = 0; h < nrOfClients && !found; h++)
							{
								if (clients[h]->socket == i)
								{
									if (clients[h]->isSpectator)
									{
										for (int k = 0; k < nrOfGames && !found; k++)
										{
											for (int j = 0; j < games[k]->nrOfSpectators && !found; j++)
											{
												if (games[k]->spectators[j]->socket == clients[h]->socket)
												{
													for (int l = j; l < games[k]->nrOfSpectators; l++)
													{
														games[k]->spectators[j] = games[k]->spectators[j + 1];
													}
													games[k]->nrOfSpectators--;

													found = true;
												}
											}
										}
									}

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
						// printf("%s", clientMsg);

						sscanf(clientMsg, "%s %[^\n]", command, buffer);
						struct sockaddr_in addres;
						socklen_t sa_len;
						sa_len = sizeof(addres);
						getpeername(i, (struct sockaddr *)&addres, &sa_len);

						char bobp[250];
						socklen_t bl = sizeof(bobp);
						inet_ntop(addres.sin_family, &addres.sin_addr, bobp, bl);
						for (int b = 0; b < nrOfClients; b++)
						{
							if (strcmp(bobp, clients[b]->address) == 0 && ntohs(addres.sin_port) == clients[b]->port)
							{

								if (strcmp(command, "CONACC") == 0 || strcmp(command, "RESET") == 0)
								{

									numBytes = send(clients[b]->socket, "MENU\0", strlen("MENU\0"), 0);
									clients[b]->inMenu = true;
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
												numBytes = send(clientQueue[0]->socket, "RDY\0", strlen("RDY\0"), 0);
												printf("[<]Sent %d bytes\n", numBytes);

												numBytes = send(clientQueue[1]->socket, "RDY\0", strlen("RDY\0"), 0);
												printf("[<]Sent %d bytes\n", numBytes);
											}
										}
										else if (strcmp(buffer, "2") == 0)
										{
											clients[b]->isSpectator = true;
											sprintf(msgToSend, "SPEC ");
											char temp[256];
											for (int k = 0; k < nrOfGames; k++)
											{
												memset(&temp, 0, sizeof(temp));
												sprintf(temp, "game %d: Score %d - %d\n", k, games[k]->player1->score, games[k]->player2->score);
												strcat(msgToSend, temp);
											}
											numBytes = send(clients[b]->socket, msgToSend, strlen(msgToSend), 0);
											printf("[<]Sent %d bytes\n", numBytes);
										}
										clients[b]->inMenu = false;
									}
									else if (clients[b]->isSpectator)
									{
										int temp = atoi(buffer);

										if (temp < nrOfGames)
										{
											games[temp]->spectators[games[temp]->nrOfSpectators++] = clients[b];
										}
									}
									else
									{
										for (int u = 0; u < nrOfGames; u++)
										{
											if (games[u]->player1->socket == i)
											{
												printf("Option selected\n");
												if (strcmp(buffer, "1") == 0)
												{
													games[u]->player1->option = ROCK;
												}
												else if (strcmp(buffer, "2") == 0)
												{
													games[u]->player1->option = PAPER;
												}
												else if (strcmp(buffer, "3") == 0)
												{
													games[u]->player1->option = SCISSORS;
												}
											}
											else if (games[u]->player2->socket == i)
											{
												printf("Option selected\n");
												if (strcmp(buffer, "1") == 0)
												{
													games[u]->player2->option = ROCK;
												}
												else if (strcmp(buffer, "2") == 0)
												{
													games[u]->player2->option = PAPER;
												}
												else if (strcmp(buffer, "3") == 0)
												{
													games[u]->player2->option = SCISSORS;
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
														games[nrOfGames]->player1 = clientQueue[j];
														games[nrOfGames]->player2 = clientQueue[k];
														threads[nrOfThreads++] = new std::thread(playGame, games[nrOfGames]);
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
	supervisorThread.join();
	return 0;
}