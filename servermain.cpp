#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <iostream>
#include <errno.h>
#include <stdint.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <regex.h>
#include <time.h>
#include <thread>
#include <pthread.h>
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#define ROCK 1
#define PAPER 2
#define SCISSORS 3
#define MAX_SPECTATORS 10
#define MAX_GAMES 10
#define MAX_CLIENTS 100
#define MAX_THREADS 10
#define MSGSIZE 256
#define BUFFERSIZE 512
#define WINMSG "MESG You won!\n"
#define LOSSMSG "MESG You lost!\n"
#define DISCOMSG "MESG A player disconnected, ending game!\n"
#define CLEARMSG "CLEARCON 1\n"
#define ERRORINPUT "EROR Wrong Input\n"
struct client
{
	char *address;
	uint16_t port, socket, score = 0, option = 0;
	bool inMenu = true, ready = false, inGame = false, isSpectator = false;
	double allResponseTime;
	clock_t lastMsg;
	clock_t timeOut;
};
struct game
{
	client *player1;
	client *player2;
	client *spectators[MAX_SPECTATORS];
	time_t startTime;
	int nrOfSpectators = 0, rounds = 0;
	bool gameOver = false;
};
int nrOfGames = 0, nrOfThreads = 0, nrOfClients = 0;
client **clients = new client *[MAX_CLIENTS] { nullptr };
game **games = new game *[MAX_GAMES] { nullptr };
std::thread **threads = new std::thread *[MAX_THREADS] { nullptr };
uint16_t numBytes;
std::vector<double> scores;
void resetPlayer(client *player)
{
	if (player)
	{

		player->option = 0;
		player->ready = false;
		player->score = 0;
		if (player->isSpectator)
		{
			player->inMenu = true;
		}
		player->isSpectator = false;
		send(player->socket, "OVER \n", strlen("OVER \n"), 0);
	}
}
void removeSpectatorFromGame(client *client)
{
	bool found;
	for (int k = 0; k < nrOfGames && !found; k++)
	{
		for (int j = 0; j < games[k]->nrOfSpectators && !found; j++)
		{
			if (games[k]->spectators[j]->socket == client->socket)
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
void removeClient(int socket)
{
	bool found = false;
	for (int h = 0; h < nrOfClients && !found; h++)
	{
		if (clients[h]->socket == socket)
		{
			if (clients[h]->inGame && clients[h]->isSpectator)
			{
				removeSpectatorFromGame(clients[h]);
			}
			else if (clients[h]->inGame)
			{
				for (int k = 0; k < nrOfGames && !found; k++)
				{
					if (games[k]->player1->socket == clients[h]->socket ||
						games[k]->player2->socket == clients[h]->socket)
					{
						games[k]->gameOver = true;
						found = true;
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
}
void checkGames(void)
{
	char player1Msg[BUFFERSIZE];
	char player2Msg[BUFFERSIZE];
	char specMsg[MSGSIZE];
	char tempMsg[MSGSIZE];
	while (true)
	{

		for (int i = 0; i < nrOfGames; i++)
		{
			if (games[i]->gameOver)
			{
				memset(&player1Msg, 0, sizeof(player1Msg));
				memset(&player2Msg, 0, sizeof(player2Msg));
				memset(&specMsg, 0, sizeof(specMsg));
				memset(&tempMsg, 0, sizeof(tempMsg));
				threads[i]->join();
				sprintf(tempMsg, "MESG Score: %d - %d\n", games[i]->player1->score, games[i]->player2->score);
				if (games[i]->player1->score >= 3)
				{
					sprintf(specMsg, "%sMESG Player 1 won, Score: %d - %d\n", CLEARMSG, games[i]->player1->score, games[i]->player2->score);
					sprintf(player1Msg, "%s%s%s", CLEARMSG, WINMSG, tempMsg);
					sprintf(player2Msg, "%s%s%s", CLEARMSG, LOSSMSG, tempMsg);
				}
				else if (games[i]->player2->score >= 3)
				{
					sprintf(specMsg, "%sMESG Player 2 won, Score: %d - %d\n", CLEARMSG, games[i]->player1->score, games[i]->player2->score);
					sprintf(player1Msg, "%s%s%s", CLEARMSG, LOSSMSG, tempMsg);
					sprintf(player2Msg, "%s%s%s", CLEARMSG, WINMSG, tempMsg);
				}
				else
				{
					sprintf(specMsg, "%sMESG A player disconnected, ending game!\n", CLEARMSG);
					sprintf(player1Msg, "%s%s", CLEARMSG, DISCOMSG);
					sprintf(player2Msg, "%s%s", CLEARMSG, DISCOMSG);
				}
				send(games[i]->player1->socket, player1Msg, strlen(player1Msg), 0);
				send(games[i]->player2->socket, player2Msg, strlen(player2Msg), 0);
				resetPlayer(games[i]->player1);
				resetPlayer(games[i]->player2);
				scores.push_back(games[i]->player1->allResponseTime / games[i]->rounds);
				scores.push_back(games[i]->player2->allResponseTime / games[i]->rounds);
				std::sort(scores.begin(), scores.end());
				for (int j = 0; j < games[i]->nrOfSpectators; j++)
				{
					send(games[i]->spectators[j]->socket, specMsg, strlen(specMsg), 0);
					resetPlayer(games[i]->spectators[j]);
				}
				delete games[i];
				delete threads[i];
				for (int j = i; j < nrOfGames; j++)
				{
					if (games[j + 1] != nullptr)
					{
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
	char specMsg[MSGSIZE];
	char player1Msg[MSGSIZE];
	char player2Msg[MSGSIZE];
	clock_t startTime, now;
	int counter = 4;
	bool startSent = false;
	while (!game->gameOver)
	{
		memset(&specMsg, 0, sizeof(specMsg));

		if (game != nullptr)
		{

			if (game->player1->score >= 3 || game->player2->score >= 3)
			{
				//This sleep is purely asthetic to delay the players return to the main menu
				usleep(500 * 1000);
				printf("Game over!\n");
				game->gameOver = true;
			}
			else
			{

				if (counter > 0)
				{
					//This while-loop makes sure 1 second passes between each message before the game starts
					time(&startTime);
					usleep(900 * 1000);
					while ((time(NULL) - startTime) < 1 && counter > 0)
						;

					counter--;
					if (counter != 0)
					{
						sprintf(specMsg, "MESG Game will start in %d seconds\n", counter);
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
					}
				}
				else if (!startSent && counter == 0)
				{

					numBytes = send(game->player1->socket, "STRT \n", strlen("STRT \n"), 0);
					game->player1->lastMsg = clock();
					numBytes = send(game->player2->socket, "STRT \n", strlen("STRT \n"), 0);
					game->player2->lastMsg = clock();
					game->player1->option = 0;
					game->player2->option = 0;
					game->player1->timeOut = clock();
					game->player2->timeOut = game->player1->timeOut;

					startSent = true;
				}
				else if (counter <= 0)
				{
					now = clock();
					if ((now - game->player1->timeOut) / CLOCKS_PER_SEC > 2 && game->player1->option == 0)
					{
						game->player2->score++;
						sprintf(player1Msg, "MESG You did not make a selection, automatic loss of round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(player2Msg, "MESG Other player didn't select, you won the round\nMESG score %d - %d\n", game->player2->score, game->player1->score);
						sprintf(specMsg, "MESG Player 1 didn't select\nMESG score %d - %d\n", game->player1->score, game->player2->score);
						game->player1->allResponseTime += 2.f;
						game->player1->option = (uint16_t)-1;
					}
					if ((now - game->player2->timeOut) / CLOCKS_PER_SEC > 2 && game->player2->option == 0)
					{
						game->player1->score++;
						sprintf(player2Msg, "MESG You did not make a selection, automatic loss of round\nMESG score %d - %d\n", game->player2->score, game->player1->score);
						sprintf(player1Msg, "MESG Other player didn't select, you won the round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
						sprintf(specMsg, "MESG Player 2 didn't select\nMESG score %d - %d\n", game->player1->score, game->player2->score);
						game->player2->allResponseTime += 2.f;
						game->player2->option = (uint16_t)-1;
					}

					if (game->player1->option > 0 && game->player2->option > 0)
					{
						if (game->player1->option == (uint16_t)-1 && game->player2->option == (uint16_t)-1)
						{
							game->player1->score--;
							game->player2->score--;
							sprintf(player1Msg, "MESG No player selected, restarting round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
							sprintf(player2Msg, "MESG No player selected, restarting round\nMESG score %d - %d\n", game->player2->score, game->player1->score);
							sprintf(specMsg, "MESG No player selected, restarting round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
						}
						else if (game->player1->option != (uint16_t)-1 && game->player2->option != (uint16_t)-1)
						{

							printf("Both have selected\n");
							switch (game->player1->option)
							{
							case ROCK:
								strcat(specMsg, "MESG Player 1 selected ROCK");
								break;
							case PAPER:
								strcat(specMsg, "MESG Player 1 selected PAPER");
								break;
							case SCISSORS:
								strcat(specMsg, "MESG Player 1 selected SCISSORS");
								break;
							}
							switch (game->player2->option)
							{
							case ROCK:
								strcat(specMsg, ", Player 2 selected ROCK\n");
								break;
							case PAPER:
								strcat(specMsg, ", Player 2 selected PAPER\n");
								break;
							case SCISSORS:
								strcat(specMsg, ", Player 2 selected SCISSORS\n");
								break;
							}
							printf("%s\n", specMsg);
							for (int i = 0; i < game->nrOfSpectators; ++i)
							{
								if (game->spectators[i])
								{
									numBytes = send(game->spectators[i]->socket, specMsg, strlen(specMsg), 0);
									printf("[<]Sent %d bytes\n", numBytes);
								}
							}
							if ((game->player1->option == ROCK && game->player2->option == SCISSORS) ||
								(game->player1->option == PAPER && game->player2->option == ROCK) ||
								(game->player1->option == SCISSORS && game->player2->option == PAPER))
							{
								printf("Player 1 has won the round\n");
								game->player1->score++;
								sprintf(player1Msg, "MESG You've won the round: Score %d - %d\n", game->player1->score, game->player2->score);
								sprintf(player2Msg, "MESG You've lost the round: Score %d - %d\n", game->player2->score, game->player1->score);

								sprintf(specMsg, "MESG Player 1 has won the round: Score %d - %d\n", game->player1->score, game->player2->score);
							}
							else if ((game->player2->option == ROCK && game->player1->option == SCISSORS) ||
									 (game->player2->option == PAPER && game->player1->option == ROCK) ||
									 (game->player2->option == SCISSORS && game->player1->option == PAPER))
							{
								printf("Player 2 has won the round\n");
								game->player2->score++;
								sprintf(player1Msg, "MESG You've lost the round: Score %d - %d\n", game->player1->score, game->player2->score);
								sprintf(player2Msg, "MESG You've won the round: Score %d - %d\n", game->player2->score, game->player1->score);
								sprintf(specMsg, "MESG Player 2 has won the round: Score %d - %d\n", game->player1->score, game->player2->score);
							}
							else
							{
								printf("It's a tie\n");
								sprintf(specMsg, "MESG It's a tie: Score %d - %d\n", game->player1->score, game->player2->score);
								sprintf(player1Msg, "MESG It's a tie: Score %d - %d\n", game->player1->score, game->player2->score);
								sprintf(player2Msg, "MESG It's a tie: Score %d - %d\n", game->player2->score, game->player1->score);
							}
						}
						send(game->player1->socket, player1Msg, strlen(player1Msg), 0);

						send(game->player2->socket, player2Msg, strlen(player2Msg), 0);


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
						counter = 4;
						startSent = false;
						game->rounds++;
					}
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

	char clientMsg[MSGSIZE];
	int clientMsgLen = sizeof(clientMsg);
	char msgToSend[MSGSIZE];
	char command[30];
	char buffer[BUFFERSIZE];
	char protocol[20] = "RPS TCP 1\n";

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
		if (select(fDMax + 1, &read_sds, NULL, NULL, NULL) == -1)
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
							printf("Client left\n");
							bool found = false;

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

							removeClient(i);

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
						sscanf(clientMsg, "%s %[^\n]", command, buffer);
						struct sockaddr_in addres;
						socklen_t sa_len;
						sa_len = sizeof(addres);
						getpeername(i, (struct sockaddr *)&addres, &sa_len);

						char bobp[BUFFERSIZE];
						socklen_t bl = sizeof(bobp);
						inet_ntop(addres.sin_family, &addres.sin_addr, bobp, bl);
						for (int b = 0; b < nrOfClients; b++)
						{
							if (strcmp(bobp, clients[b]->address) == 0 && ntohs(addres.sin_port) == clients[b]->port)
							{
								if (strcmp(command, "CONA") == 0 || strcmp(command, "RSET") == 0)
								{
									if (clients[b]->isSpectator)
									{
										removeSpectatorFromGame(clients[b]);
									}

									numBytes = send(clients[b]->socket, "MENU\0", strlen("MENU\0"), 0);
									clients[b]->inMenu = true;
									clients[b]->isSpectator = false;
									clients[b]->inGame = false;
									clients[b]->lastMsg = clock();
									printf("[<]Sent %d bytes\n", numBytes);
								}
								else if (strcmp(command, "OPTN") == 0)
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
												numBytes = send(clientQueue[0]->socket, "REDY\0", strlen("REDY\0"), 0);
												clientQueue[0]->lastMsg = clock();
												printf("[<]Sent %d bytes\n", numBytes);

												numBytes = send(clientQueue[1]->socket, "REDY\0", strlen("REDY\0"), 0);
												clientQueue[1]->lastMsg = clock();
												printf("[<]Sent %d bytes\n", numBytes);
											}
											else
											{
												send(clientQueue[0]->socket, "MESG One more player required to start game\n", strlen("MESG One more player required to start game\n"), 0);
											}
										}
										else if (strcmp(buffer, "2") == 0)
										{
											clients[b]->isSpectator = true;
											sprintf(msgToSend, "SPEC %d\n", nrOfGames);
											char temp[BUFFERSIZE];
											for (int k = 0; k < nrOfGames; k++)
											{
												memset(&temp, 0, sizeof(temp));
												sprintf(temp, "SPEC game %d: Score %d - %d\n", k + 1, games[k]->player1->score, games[k]->player2->score);
												strcat(msgToSend, temp);
											}
											numBytes = send(clients[b]->socket, msgToSend, strlen(msgToSend), 0);
											clients[b]->lastMsg = clock();

											clients[b]->inMenu = false;
											printf("[<]Sent %d bytes\n", numBytes);
										}
										else if (strcmp(buffer, "3") == 0)
										{
											char temp[BUFFERSIZE];

											sprintf(msgToSend, "SCORE %d\n", (int)scores.size());
											for (int k = 0; k < (int)scores.size(); k++)
											{
												memset(&temp, 0, sizeof(temp));
												sprintf(temp, "SCORE %lf\n", scores[k]);
												strcat(msgToSend, temp);
											}
											numBytes = send(clients[b]->socket, msgToSend, strlen(msgToSend), 0);
											clients[b]->lastMsg = clock();

											printf("[<]Sent %d bytes\n", numBytes);

											clients[b]->inMenu = false;
										}
										else if (strcmp(buffer, "0") == 0)
										{
											send(clients[b]->socket, "QUIT 1\n", strlen("QUIT 1\n"), 0);
											removeClient(clients[b]->socket);
											std::cout << "Client left\nClients left: " << nrOfClients << std::endl;
											close(i);
											FD_CLR(i, &master);
										}
										else
										{

											send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
										}
									}
									else if (clients[b]->isSpectator)
									{
										int temp = atoi(buffer);
										temp--;
										std::cout << temp << std::endl;
										if (temp < nrOfGames && temp >= 0)
										{
											games[temp]->spectators[games[temp]->nrOfSpectators++] = clients[b];
											send(clients[b]->socket, "SPECACC \n", strlen("SPECACC \n"), 0);
										}
										else
										{
											send(clients[b]->socket, "SPECREJ \n", strlen("SPECREJ \n"), 0);
										}
									}
									else
									{

										for (int u = 0; u < nrOfGames; u++)
										{
											if (games[u]->player1->socket == i)
											{
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
												else
												{
													send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
												}

												double responseTimeInSec = (double)(clock() - games[u]->player1->lastMsg) / CLOCKS_PER_SEC;
												games[u]->player1->allResponseTime += responseTimeInSec;
											}
											else if (games[u]->player2->socket == i)
											{
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
												else
												{
													send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
												}

												double responseTimeInSec = (double)(clock() - games[u]->player2->lastMsg) / CLOCKS_PER_SEC;
												games[u]->player2->allResponseTime += responseTimeInSec;
											}
										}
									}
								}
								else if (strcmp(command, "REDY") == 0)
								{
									clients[b]->ready = true;
									if (clientsInQueue >= 2)
									{
										bool found = false;
										for (int j = 0; j < clientsInQueue && !found; j++)
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
														clientQueue[j]->inGame = true;
														clientQueue[k]->inGame = true;
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
									send(clients[b]->socket, "EROR Unkown command\n", strlen("EROR Unkown command\n"), 0);
								}
							}
						}
					}
				}
			}
		}
	}
	for (int i = 0; i < nrOfGames; i++)
	{
		delete [] games[i]->spectators;
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