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
#include <cstdint>
#include <chrono>
#include <vector>
#include <algorithm>
#define TIMEOUT 2
#define ROCK 1
#define PAPER 2
#define SCISSORS 3
#define MAX_SPECTATORS 10
#define MAX_GAMES 10
#define MAX_CLIENTS 100
#define MAX_THREADS 10
#define MSGSIZE 256
#define BUFFERSIZE 512
#define WINMSG "MESG You won!\nMESG Score %d - %d\n"
#define LOSSMSG "MESG You lost!\nMESG Score %d - %d\n"
#define DISCOMSG "MESG A player disconnected, ending game!\n"
#define CLEARMSG "CLEARCON 1\n"
#define ERRORINPUT "EROR Wrong Input\n"
#define PLAYINPUT "1"
#define WATCHINPUT "2"
#define SCOREINPUT "3"
#define EXITINPUT "0"
#define ROCKINPUT "1"
#define PAPERINPUT "2"
#define SCISSORINPUT "3"
struct client
{
	char *address;
	uint16_t port, socket, score = 0, option = 0;
	bool inMenu = true, readySent = false, ready = false, inGame = false, wantsToQueue = false, wantsToLeaveQueue = false, inQueue = false, wantsToSpec = false,
		 isSpectator = false, selected = false, started = false, disconnected = false;
	double allResponseTime;
	timeval lastMsg;
};
struct game
{
	client *player1, *player2, *spectators[MAX_SPECTATORS];
	clock_t startTime;
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
		player->started = false;
		player->selected = false;
		player->score = 0;
		player->inGame = false;
		player->wantsToQueue = false;
		player->inQueue = false;
		player->readySent = false;
		send(player->socket, "OVER 1\n", strlen("OVER 1\n"), 0);
		gettimeofday(&player->lastMsg, NULL);
		player->allResponseTime = 0.f;
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
				client->inMenu = true;
				client->isSpectator = false;
			}
		}
	}
}
void deleteClient(const int &index)
{
	client *temp = clients[index];
	for (int j = index; j < nrOfClients; j++)
	{
		if (clients[j + 1] != nullptr)
		{

			clients[j] = clients[j + 1];
		}
	}
	nrOfClients--;
	delete temp;
	std::cout << "Clients left: " << nrOfClients << std::endl;
}
//This thread handles clients that disconnect. It waits until their game is finished before deleting them safely
void disconnectionHandler()
{
	bool marked = false;
	while (true)
	{
		for (int i = 0; i < nrOfClients; ++i)
		{
			if (clients[i]->disconnected)
			{
				if (clients[i]->isSpectator)
				{
					removeSpectatorFromGame(clients[i]);
					deleteClient(i);
				}
				else if (clients[i]->inGame || clients[i]->inQueue)
				{
					if (clients[i]->inQueue)
					{
						marked = true;
					}
					else
					{
						for (int j = 0; j < nrOfGames && !marked; ++j)
						{
							if (clients[i] == games[j]->player1 || clients[i] == games[j]->player2)
							{
								games[j]->gameOver = true;
								marked = true;
							}
						}
					}
				}
				else
				{
					marked = false;
					deleteClient(i);
				}
			}
		}
	}
}
//This thread handles games that are over, either through disconnects or regular play
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
					sprintf(specMsg, "MESG Player 1 won, Score: %d - %d\n", games[i]->player1->score, games[i]->player2->score);
					sprintf(player1Msg, WINMSG, games[i]->player1->score, games[i]->player2->score);
					sprintf(player2Msg, LOSSMSG, games[i]->player2->score, games[i]->player1->score);
					scores.push_back(games[i]->player1->allResponseTime / games[i]->rounds);
					std::sort(scores.begin(), scores.end());
				}
				else if (games[i]->player2->score >= 3)
				{
					sprintf(specMsg, "MESG Player 2 won, Score: %d - %d\n", games[i]->player1->score, games[i]->player2->score);
					sprintf(player1Msg, LOSSMSG, games[i]->player1->score, games[i]->player2->score);
					sprintf(player2Msg, WINMSG, games[i]->player2->score, games[i]->player1->score);
					scores.push_back(games[i]->player2->allResponseTime / games[i]->rounds);
					std::sort(scores.begin(), scores.end());
				}
				else
				{
					sprintf(specMsg, "MESG A player disconnected, ending game and returning to game select screen!\n");
					sprintf(player1Msg, "%s", DISCOMSG);
					sprintf(player2Msg, "%s", DISCOMSG);
				}

				resetPlayer(games[i]->player1);
				resetPlayer(games[i]->player2);
				if (!games[i]->player1->disconnected)
				{
					send(games[i]->player1->socket, player1Msg, strlen(player1Msg), 0);
				}
				if (!games[i]->player2->disconnected)
				{
					send(games[i]->player2->socket, player2Msg, strlen(player2Msg), 0);
				}
				for (int j = 0; j < games[i]->nrOfSpectators; j++)
				{
					resetPlayer(games[i]->spectators[j]);
					send(games[i]->spectators[j]->socket, specMsg, strlen(specMsg), 0);
				}
				game *tempGame = games[i];
				std::thread *tempThread = threads[i];
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
				delete tempGame;
				delete tempThread;
			}
		}
	}
}
//This thread handles the queue of players waiting to join, it also creates games when enough players are ready
void playGame(game *game)
{
	char specMsg[MSGSIZE];
	char player1Msg[MSGSIZE];
	char player2Msg[MSGSIZE];
	time_t start;
	int counter = 4;
	send(game->player1->socket, "MESG Game is now starting!\n", strlen("MESG Game is now starting\n"), 0);
	send(game->player2->socket, "MESG Game is now starting!\n", strlen("MESG Game is now starting\n"), 0);
	 while (!game->gameOver)
	{
		memset(&player1Msg, 0, sizeof(player1Msg));
		memset(&player2Msg, 0, sizeof(player2Msg));
		memset(&specMsg, 0, sizeof(specMsg));

		if (game != nullptr && !game->player1->disconnected && !game->player2->disconnected)
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
					time(&start);
					usleep(900 * 1000);
					while ((time(NULL) - start) < 1 && counter > 0)
						;
					counter--;
					if (counter > 0)
					{
						sprintf(specMsg, "MESG Game will start in %d seconds\n", counter);
						numBytes = send(game->player1->socket, specMsg, strlen(specMsg), 0);

						numBytes = send(game->player2->socket, specMsg, strlen(specMsg), 0);
						for (int i = 0; i < game->nrOfSpectators; i++)
						{
							if (game->spectators[i])
							{
								numBytes = send(game->spectators[i]->socket, specMsg, strlen(specMsg), 0);
							}
						}
					}
					else
					{
						game->rounds++;
						char roundStr[BUFFERSIZE];
						sprintf(roundStr, "MESG Round %d\nMESG Timer Started\n", game->rounds);
						std::cout << "Round " << game->rounds << std::endl;
						send(game->player1->socket, roundStr, strlen(roundStr), 0);
						send(game->player1->socket, "STRT \n", strlen("STRT \n"), 0);
						send(game->player2->socket, roundStr, strlen(roundStr), 0);
						send(game->player2->socket, "STRT \n", strlen("STRT \n"), 0);
						for (int i = 0; i < game->nrOfSpectators; i++)
						{
							send(game->spectators[i]->socket, roundStr, strlen(roundStr), 0);
						}
						game->player1->option = 0;
						game->player1->selected = false;
						game->player1->started = true;
						game->player2->selected = false;
						game->player2->started = true;
						game->player2->option = 0;

						gettimeofday(&game->player1->lastMsg, NULL);
						game->player2->lastMsg = game->player1->lastMsg;

						time(&start);
					}
				}
				else
				{
					if ((time(NULL) - start) >= TIMEOUT)
					{
						if (game->player1->option == 0)
						{
							game->player2->score++;
							game->player1->selected = true;
							game->player1->started = false;
							std::cout << "Player 1 timed out\n";
							sprintf(player1Msg, "MESG You did not make a selection, automatic loss of round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
							sprintf(player2Msg, "MESG Other player didn't select, you won the round\nMESG score %d - %d\n", game->player2->score, game->player1->score);
							sprintf(specMsg, "MESG Player 1 didn't select, Player 2 Wins\nMESG score %d - %d\n", game->player1->score, game->player2->score);
							game->player1->allResponseTime += (double)TIMEOUT;
							game->player1->option = (uint16_t)-1;
						}
						if (game->player2->option == 0)
						{
							game->player1->score++;
							game->player2->selected = true;
							game->player2->started = false;
							std::cout << "Player 2 timed out\n";
							sprintf(player2Msg, "MESG You did not make a selection, automatic loss of round\nMESG score %d - %d\n", game->player2->score, game->player1->score);
							sprintf(player1Msg, "MESG Other player didn't select, you won the round\nMESG score %d - %d\n", game->player1->score, game->player2->score);
							sprintf(specMsg, "MESG Player 2 didn't select, Player 1 wins\nMESG score %d - %d\n", game->player1->score, game->player2->score);
							game->player2->allResponseTime += (double)TIMEOUT;
							game->player2->option = (uint16_t)-1;
						}
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
							for (int i = 0; i < game->nrOfSpectators; ++i)
							{
								if (game->spectators[i])
								{
									numBytes = send(game->spectators[i]->socket, specMsg, strlen(specMsg), 0);
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
							}
						}

						game->player1->option = 0;
						game->player2->option = 0;
						game->player1->started = false;
						game->player2->started = false;
						counter = 4;
					}
				}
			}
		}
	}
}
void sendSpec(client *theClient)
{
	char msgToSend[BUFFERSIZE];
	theClient->wantsToSpec = true;
	memset(msgToSend, 0, sizeof(msgToSend));
	if (nrOfGames > 0)
	{
		char temp[BUFFERSIZE];
		for (int k = 0; k < nrOfGames; k++)
		{
			memset(&temp, 0, sizeof(temp));
			sprintf(temp, "SPEC game %d: Score %d - %d\n", k + 1, games[k]->player1->score, games[k]->player2->score);
			strcat(msgToSend, temp);
		}
	}
	else
	{
		strcat(msgToSend, "SPEC NONE\n");
	}
	strcat(msgToSend, "SPECDONE 1\n");
	numBytes = send(theClient->socket, msgToSend, strlen(msgToSend), 0);
	theClient->inMenu = false;
}
void queueHandler(void)
{
	client *clientQueue[10];
	int clientsInQueue = 0, nrOfReadyClients = 0;
	while (true)
	{
		for (int i = 0; i < nrOfClients; ++i)
		{
			if ((clients[i]->disconnected && clients[i]->inQueue) || clients[i]->wantsToLeaveQueue)
			{
				for (int j = 0; j < clientsInQueue; ++j)
				{
					if (clients[i] == clientQueue[j])
					{
						if (clients[i]->readySent)
						{
							nrOfReadyClients--;
						}
						if (nrOfReadyClients == 1)
						{
							bool found = false;
							for (int l = 0; l < clientsInQueue && !found; l++)
							{
								if ((clientQueue[l]->ready || clientQueue[l]->readySent) && clientQueue[l] != clients[i])
								{
									found = true;
									clientQueue[l]->ready = false;
									clientQueue[l]->readySent = false;
									send(clientQueue[l]->socket, "MESG The other player disconnected, returning to queue\n", strlen("MESG The other player disconnected, returning to queue\n"), 0);
									nrOfReadyClients--;
								}
							}
						}
						for (int l = j; l < clientsInQueue; l++)
						{
							clientQueue[l] = clientQueue[l + 1];
						}

						clientsInQueue--;
						clients[i]->inQueue = false;
						clients[i]->wantsToLeaveQueue = false;
						clients[i]->inMenu = true;
					}
				}
			}
			if (clients[i]->wantsToQueue && clientsInQueue < 10)
			{
				clientQueue[clientsInQueue++] = clients[i];
				if (clientsInQueue - nrOfReadyClients < 2)
				{
					send(clients[i]->socket, "MESG One more player required to start game\nMESG Press \"Enter\" to leave the queue\n", strlen("MESG One more player required to start game\nMESG Press \"Enter\" to leave the queue\n"), 0);
				}
				clients[i]->wantsToQueue = false;
				clients[i]->inQueue = true;
				clients[i]->inMenu = false;
			}
		}
		if (clientsInQueue - nrOfReadyClients >= 2)
		{
			for (int i = 0; i < clientsInQueue; ++i)
			{
				if (!clientQueue[i]->readySent && !clientQueue[i]->ready)
				{
					send(clientQueue[i]->socket, "READY 1\n", strlen("READY 1\n"), 0);
					clientQueue[i]->readySent = true;
					nrOfReadyClients++;
				}
			}
		}
		if (nrOfReadyClients >= 2)
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
							std::cout << "Creating a new game\n";
							found = true;
							games[nrOfGames] = new game;
							games[nrOfGames]->player1 = clientQueue[j];
							games[nrOfGames]->player2 = clientQueue[k];
							threads[nrOfThreads++] = new std::thread(playGame, games[nrOfGames]);
							clientQueue[j]->inGame = true;
							clientQueue[j]->inQueue = false;
							clientQueue[j]->ready = false;
							clientQueue[j]->readySent = false;
							clientQueue[k]->inGame = true;
							clientQueue[k]->ready = false;
							clientQueue[k]->readySent = false;
							clientQueue[k]->inQueue = false;
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
							nrOfReadyClients -= 2;
							if (nrOfReadyClients == 1)
							{
								bool found = false;
								for (int l = 0; l < clientsInQueue && !found; l++)
								{
									if ((clientQueue[l]->ready || clientQueue[l]->readySent))
									{
										found = true;
										clientQueue[l]->ready = false;
										clientQueue[l]->readySent = false;
										send(clientQueue[l]->socket, "MESG The other player disconnected, returning to queue\n", strlen("MESG The other player disconnected, returning to queue\n"), 0);
										nrOfReadyClients--;
									}
								}
							}
						}
					}
				}
			}
		}
	}
}
int main(int argc, char *argv[])
{

	if (argc < 3)
	{
		printf("To few arguments!\nExpected <IP> <port>\n");
		exit(0);
	}

	struct addrinfo guide, *serverInfo, *p;
	struct sockaddr_in theirAddr;
	socklen_t theirAddr_len = sizeof(theirAddr);

	uint8_t returnValue;
	uint16_t sockFD, tempSFD;

	fd_set master, read_sds;

	char clientMsg[MSGSIZE], msgToSend[MSGSIZE], buffer[BUFFERSIZE], command[30], protocol[20] = "RPS TCP 1\n";

	int clientMsgLen = sizeof(clientMsg), fDMax;

	FD_ZERO(&master);
	FD_ZERO(&read_sds);

	memset(&guide, 0, sizeof(guide));
	guide.ai_family = AF_INET;
	guide.ai_socktype = SOCK_STREAM;
	guide.ai_flags = AI_PASSIVE;

	if ((returnValue = getaddrinfo(argv[1], argv[2], &guide, &serverInfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(returnValue));
		exit(0);
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
	std::thread disconnectionThread(disconnectionHandler);
	std::thread queueThread(queueHandler);
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
						numBytes = send(clients[nrOfClients]->socket, protocol, strlen(protocol), 0);

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
							for (int k = 0; k < nrOfClients && !found; ++k)
							{
								if (clients[k]->socket == i)
								{
									found = true;
									clients[k]->disconnected = true;
								}
							}

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
								if (strcmp(command, "CONACC") == 0 || strcmp(command, "RESET") == 0)
								{

									if (clients[b]->isSpectator)
									{
										std::cout << "The spectator\n";
										removeSpectatorFromGame(clients[b]);
										clients[b]->isSpectator = false;
										sendSpec(clients[b]);
									}
									else
									{
										std::cout << "sending to menu\n";
										numBytes = send(clients[b]->socket, "MENU 1\0", strlen("MENU 1\0"), 0);
										clients[b]->inMenu = true;
										clients[b]->wantsToSpec = false;
									}
									gettimeofday(&clients[b]->lastMsg, NULL);
									clients[b]->inGame = false;
									clients[b]->isSpectator = false;
								}
								else if (strcmp(command, "OPTION") == 0)
								{
									if (clients[b]->inMenu == true)
									{
										if (strcmp(buffer, PLAYINPUT) == 0)
										{
											clients[b]->inMenu = false;
											clients[b]->wantsToQueue = true;
										}
										else if (strcmp(buffer, WATCHINPUT) == 0)
										{
											sendSpec(clients[b]);
										}
										else if (strcmp(buffer, SCOREINPUT) == 0)
										{
											if (scores.size() > 0)
											{
												char temp[BUFFERSIZE];
												std::cout << scores.size() << std::endl;
												for (int k = 0; k < (int)scores.size(); k++)
												{
													memset(&temp, 0, sizeof(temp));
													sprintf(temp, "SCORE %lf seconds\n", scores[k]);
													strcat(msgToSend, temp);
												}
											}
											else
											{
												sprintf(msgToSend, "SCORE NONE\n");
											}

											strcat(msgToSend, "SCOREDONE 1\n");
											send(clients[b]->socket, msgToSend, strlen(msgToSend), 0);
											clients[b]->inMenu = false;
										}
										else if (strcmp(buffer, EXITINPUT) == 0)
										{
											send(clients[b]->socket, "QUIT 1\n", strlen("QUIT 1\n"), 0);
											clients[b]->disconnected = true;
											close(i);
											FD_CLR(i, &master);
										}
										else
										{
											send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
										}
									}
									else if (clients[b]->inQueue && !clients[b]->readySent)
									{
										send(clients[b]->socket, "MENU 1\0", strlen("MENU 1\0"), 0);
										clients[b]->wantsToLeaveQueue = true;
									}
									else if (clients[b]->wantsToSpec)
									{
										int temp = atoi(buffer) - 1;
										if (temp < nrOfGames && temp >= 0)
										{
											games[temp]->spectators[games[temp]->nrOfSpectators++] = clients[b];
											clients[b]->isSpectator = true;
											send(clients[b]->socket, "SPECACC 1\n", strlen("SPECACC 1\n"), 0);
										}
										else
										{
											send(clients[b]->socket, "SPECREJ 1\n", strlen("SPECREJ 1\n"), 0);
										}
										clients[b]->wantsToSpec = false;
									}
									else if (clients[b]->isSpectator)
									{
										removeSpectatorFromGame(clients[b]);
										sendSpec(clients[b]);
									}
									else if (clients[b]->readySent)
									{
										clients[b]->ready = true;
										send(clients[b]->socket, "MESG You are now ready!\n", strlen("MESG You are now ready!\n"), 0);
									}
									else
									{

										//If the players are in-game and selects and option, they want to select ROCK,PAPER or SCISSORS
										for (int u = 0; u < nrOfGames; u++)
										{
											if (games[u]->player1->socket == i)
											{
												if (games[u]->player1->started)
												{
													timeval responseTime;
													gettimeofday(&responseTime, NULL);
													double responseTimeInSec = (responseTime.tv_sec - games[u]->player1->lastMsg.tv_sec) + (double)(responseTime.tv_usec - games[u]->player1->lastMsg.tv_usec) / 1000000.f;
													if (strcmp(buffer, ROCKINPUT) == 0)
													{
														games[u]->player1->option = ROCK;
													}
													else if (strcmp(buffer, PAPERINPUT) == 0)
													{
														games[u]->player1->option = PAPER;
													}
													else if (strcmp(buffer, SCISSORINPUT) == 0)
													{
														games[u]->player1->option = SCISSORS;
													}
													else
													{
														send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
													}

													if (!games[u]->player1->selected && games[u]->player1->option > 0)
													{
														games[u]->player1->selected = true;

														std::cout << "Player 1 registered, response time: " << responseTimeInSec << std::endl;
														games[u]->player1->allResponseTime += responseTimeInSec;
													}
												}
											}
											else if (games[u]->player2->socket == i)
											{
												if (games[u]->player2->started)
												{
													timeval responseTime;
													gettimeofday(&responseTime, NULL);
													double responseTimeInSec = (responseTime.tv_sec - games[u]->player2->lastMsg.tv_sec) + ((double)(responseTime.tv_usec - games[u]->player2->lastMsg.tv_usec) / 1000000.f);
													if (strcmp(buffer, ROCKINPUT) == 0)
													{
														games[u]->player2->option = ROCK;
													}
													else if (strcmp(buffer, PAPERINPUT) == 0)
													{
														games[u]->player2->option = PAPER;
													}
													else if (strcmp(buffer, SCISSORINPUT) == 0)
													{
														games[u]->player2->option = SCISSORS;
													}
													else
													{
														send(clients[b]->socket, ERRORINPUT, strlen(ERRORINPUT), 0);
													}

													if (!games[u]->player2->selected && games[u]->player2->option > 0)
													{
														games[u]->player2->selected = true;

														games[u]->player2->allResponseTime += responseTimeInSec;

														std::cout << "Player 2 registered, response time: " << responseTimeInSec << std::endl;
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
	for (int i = 0; i < nrOfThreads; i++)
	{
		threads[i]->join();
		delete threads[i];
	}
	delete[] games;
	for (int i = 0; i < nrOfGames; i++)
	{
		delete games[i];
	}
	delete[] games;
	for (int i = 0; i < nrOfClients; i++)
	{
		close(clients[i]->socket);
		clients[i]->disconnected = true;
	}
	delete[] clients;
	supervisorThread.join();
	disconnectionThread.join();
	return 0;
}