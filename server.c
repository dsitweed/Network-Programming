#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <signal.h>
#include "utils/utils.h"

#define MAX_CLIENTS 100
#define MAX_ROOM 100
#define BUFFER_SZ 2048
#define MAX_THREADS 10

// Tránh xung đột tranh dành ghi dữ liệu 
// (Kiểu 2 thàng cùng cli_count++ (0 -> 1 trong khi có 2 đứa))
static _Atomic unsigned int cli_count = 0; 
// id of Client
static int uid = 0;

DoubleLink *listRoom[MAX_ROOM];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void str_trim_lf (char* arr) {
  int i = strlen(arr) - 1;
  if (arr[i] == '\n' && arr[i + 1] == '\0') {
	arr[i] = '\0';
  }
}

void print_client_addr(struct sockaddr_in addr){
    printf("%d.%d.%d.%d",
        addr.sin_addr.s_addr & 0xff,
        (addr.sin_addr.s_addr & 0xff00) >> 8,
        (addr.sin_addr.s_addr & 0xff0000) >> 16,
        (addr.sin_addr.s_addr & 0xff000000) >> 24);
}

/* Send message to all clients except sender */
void send_message(char *s, int uid){
	pthread_mutex_lock(&clients_mutex);

	Node *run;

	// Cần xem lại vòng lặp 
	// need what is room
	for(int i = 0; i < MAX_ROOM; i++){
		if(listRoom[i]){
			run = listRoom[i]->head;
			while (run != NULL) {
				if (run->client.uid != uid) {
					if (send(run->client.uid, s, strlen(s), 0) <= 0) {
						perror("ERROR: write to descriptor failed");
						break;
					}
				}
				run = run->next;
			}// end for send mess to each person in 1 room	
		}// check is right room what send mess
	}// end for traverse all rooms

	free(run);
	pthread_mutex_unlock(&clients_mutex);
}

/* Handle all communication with the client */
void* handle_client(void *arg) {
	client_t cli = *(client_t *) arg; // client

	char buff_out[BUFFER_SZ];
	char name[32];
	int leave_flag = 0;

	cli_count++;

	// Name
	if(recv(cli.sockfd, name, 32, 0) <= 0 || strlen(name) <  2 || strlen(name) >= 32-1){
		printf("Didn't enter the name.\n");
		leave_flag = 1;
	} else{
		strcpy(cli.name, name);
		sprintf(buff_out, "%s has joined\n", cli.name);
		printf("%s", buff_out);
		send_message(buff_out, cli.uid);
	}

	bzero(buff_out, BUFFER_SZ);

	while(1){
		if (leave_flag) {
			break;
		}

		int receive = recv(cli.sockfd, buff_out, BUFFER_SZ, 0);
		if (receive > 0){
			if (strlen(buff_out) > 0){
				send_message(buff_out, cli.uid);

				str_trim_lf(buff_out);
				printf("%s -> %s\n", buff_out, cli.name);
			}
		} else if (receive == 0 || strcmp(buff_out, "exit") == 0){
			sprintf(buff_out, "%s has left\n", cli.name);
			printf("%s", buff_out);
			send_message(buff_out, cli.uid);
			leave_flag = 1;
		} else {
			printf("ERROR: -1\n");
			leave_flag = 1;
		}

		bzero(buff_out, BUFFER_SZ);
	}

  /* Delete client from queue and yield thread */
	close(cli.sockfd);
	deleteClient(listRoom[0], cli.uid);
	cli_count--;
	pthread_detach(pthread_self());

	return NULL;
}


int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	DoubleLink *room1 = createLinkList();
	listRoom[0] = room1;

	char *IP = "127.0.0.1";
	int PORT = atoi(argv[1]);
	int option = 1;
	int listenfd = 0, connfd = 0;
	struct sockaddr_in serv_addr;
	struct sockaddr_in cli_addr;
	pthread_t tid;

	/* Socket settings */
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(IP);
	serv_addr.sin_port = htons(PORT);

	/* Ignore pipe signals */
	signal(SIGPIPE, SIG_IGN);

	/*Tránh được lỗi "Address / Port already in use"*/
	if(setsockopt(listenfd, SOL_SOCKET,(SO_REUSEPORT | SO_REUSEADDR),(char*)&option,sizeof(option)) < 0){
		perror("ERROR: setsockopt failed");
    return EXIT_FAILURE;
	}

	/* Bind */
  	if(bind(listenfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0) {
		perror("ERROR: Socket binding failed");
		return EXIT_FAILURE;
  	}

	/* Listen request from client */
	if (listen(listenfd, MAX_THREADS) < 0) {
		perror("ERROR: Socket listening failed");
		return EXIT_FAILURE;
	}

	printf("=== WELCOME TO THE CHATROOM ===\n");

	/*Communicate with client*/
	while(1) {
		socklen_t cliAddrLen = sizeof(cli_addr);
		/*Accept*/
		connfd = accept(listenfd, (struct sockaddr*) &cli_addr, &cliAddrLen);

		/* Check if max clients is reached */
		if((cli_count + 1) == MAX_CLIENTS) {
			printf("Max clients reached. Rejected: ");
			print_client_addr(cli_addr);
			printf(":%d\n", cli_addr.sin_port);
			close(connfd);
			continue;
		}

		/* Client settings */
		client_t client;
		client.address = cli_addr;
		client.sockfd = connfd;
		client.uid = uid++;

		/* Add client to the queue and fork thread */
		insertAtHead(room1, client);
		pthread_create(&tid, NULL, handle_client, &client);

		/* Reduce CPU usage */
		sleep(1);
	}

	return EXIT_SUCCESS;
}