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
/* You will to add includes here */






using namespace std;
struct client
{
	char* address;
	char nickname[100];
	uint16_t port;
	uint16_t socket;
	int NicksChanged = 0;
};
int MAXCLIENTS = 10;
int nrOfClients = 0;
struct client** clients = new client * [MAXCLIENTS] { nullptr };
int main(int argc, char* argv[]) {
	if (argc < 2)
	{
		printf("To few arguments!\nExpected <IP(optional)> <port>\n");
		exit(0);
	}
	/* Do more magic */
	struct addrinfo guide, * serverInfo, * p;
	uint16_t numBytes;
	uint16_t sockFD;
	uint16_t tempSFD;
	uint8_t returnValue;
	regex_t regex;
	returnValue = regcomp(&regex, "^[ _[:alnum:]]*$", 0);
	if (returnValue != 0)
	{
		printf("Error compiling regex\n");
		exit(0);
	}
	fd_set master;
	fd_set read_sds;
	int fDMax;
	char clientMsg[350];
	int clientMsgLen = sizeof(clientMsg);
	char msgToSend[350];
	struct timeval timeO;
	timeO.tv_sec = 5;
	timeO.tv_usec = 0;
	memset(&guide, 0, sizeof(guide));
	FD_ZERO(&master);
	FD_ZERO(&read_sds);
	guide.ai_family = AF_INET;
	guide.ai_socktype = SOCK_STREAM;
	guide.ai_flags = AI_PASSIVE;
	struct sockaddr_in theirAddr;
	char protocol[20] = "HELLO 1\n";
	char command[10];
	char buffer[256];
	socklen_t theirAddr_len = sizeof(theirAddr);
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
						printf("Cienlt connected from %s:%d\n", inet_ntoa(theirAddr.sin_addr), ntohs(theirAddr.sin_port));
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

						}
						close(i);
						FD_CLR(i, &master);
					}
					else
					{

						sscanf(clientMsg, "%s %[^\n]", command, buffer);

						struct sockaddr_in addres;
						int sa_len;
						sa_len = sizeof(addres);
						getpeername(i, (struct sockaddr*) & addres, &sa_len);

						char bobp[250];
						socklen_t bl = sizeof(bobp);
						inet_ntop(addres.sin_family, &addres.sin_addr, bobp, bl);
						for (int i = 0; i < nrOfClients; i++)
						{
							if (strcmp(bobp, clients[i]->address) == 0 && ntohs(addres.sin_port) == clients[i]->port)
							{
								if (strcmp(command, "MSG") == 0)
								{

									sprintf(msgToSend, "%s %s %s", command,clients[i]->nickname, buffer);
									msgToSend[strlen(msgToSend)] = '\n';
								}
								else if (strcmp(command, "NICK") == 0)
								{
									if (strlen(buffer) < 12)
									{

										if (clients[i]->NicksChanged != 0)
										{

											printf("%s wants to change name\n", clients[i]->nickname);
										}
										returnValue = regexec(&regex, buffer, 0, NULL, 0);
										if (returnValue == REG_NOMATCH)
										{
											printf("Name does not match\n");
											char error[24] = "ERROR Name not allowed\n";
											send(clients[i]->socket, error, strlen(error), 0);



										}
										else
										{
											printf("Name is allowed\n");
											if (clients[i]->NicksChanged < 2)
											{
												strcpy(clients[i]->nickname, buffer);
												if (clients[i]->NicksChanged != 0)
												{

													printf("Name successfully changed\n");
												}
												clients[i]->NicksChanged++;
												char ok[4] = "OK\n";
												send(clients[i]->socket, ok, strlen(ok), 0);
											}
											else
											{
												char error[25] = "ERROR Too many changes\n";
												printf("Error, name already changed\n");
												send(clients[i]->socket, error, strlen(error), 0);

											}
										}
									}
									else
									{
										char error[14] = "ERROR Length\n";
										send(clients[i]->socket, error, strlen(error), 0);

									}

								}
								else
								{
									send(clients[i]->socket, "Unkown command\n", strlen("Unkown command\n"), 0);
								}

							}
						}
						for (int y = 0; y < nrOfClients; y++)
						{

							if (FD_ISSET(clients[y]->socket, &master))
							{
								if (y != sockFD && clients[y]->socket != i)
								{
									if (strcmp(command, "MSG") == 0)
									{

										if ((numBytes = send(clients[y]->socket, msgToSend, strlen(msgToSend), 0)) == -1)
										{
											printf("Error sending\n");
										}
									}
								}
								else
								{
									printf("%s", msgToSend);

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
