#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h> // signal
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "utils/utils.h"

#define LENGTH 2048

// Global variables
volatile sig_atomic_t flag = 0;
int sockfd = 0; // socket of this client
char name[32];

// Delete \n when use fgets
void str_trim_lf (char *arr);
void catch_ctrl_c_and_exit(int sig);
// Erase line text from terminal
void erase_line_text();
void send_msg_handler();
void recv_msg_handler();


int main(int argc, char **argv){
	if(argc != 2){
		printf("Usage: %s <port>\n", argv[0]);
		return EXIT_FAILURE;
	}

	int err;

	struct sockaddr_in server_addr;
	pthread_t send_msg_thread;
	pthread_t recv_msg_thread;
	char *IP = "127.0.0.1";
	int PORT = atoi(argv[1]);

	// Cần xem thêm 
	signal(SIGINT, catch_ctrl_c_and_exit);

	printf("Please enter your name: ");
	fgets(name, 32, stdin);

	str_trim_lf(name);


	if (strlen(name) > 32 || strlen(name) < 2){
		printf("Name must be less than 30 and more than 2 characters.\n");
		return EXIT_FAILURE;
	}

	/* Socket settings */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = inet_addr(IP);
	server_addr.sin_port = htons(PORT);

	// Connect to Server
	err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if (err == -1) {
		PRINT_ERROR;
		printf("ERROR: connect\n");
		return EXIT_FAILURE;
	}

	// Send name
	send(sockfd, name, 32, 0);

	printf("=== WELCOME TO THE CHATROOM ===\n");

	if (pthread_create(&send_msg_thread, NULL, (void *) send_msg_handler, NULL) != 0) {
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}
	
  	if(pthread_create(&recv_msg_thread, NULL, (void *) recv_msg_handler, NULL) != 0){
		printf("ERROR: pthread\n");
		return EXIT_FAILURE;
	}

	// Cách hoạt động của flag
	// Flag là cờ đánh dấu kết thúc đoạn chat 
	while (1) {
		if (flag) {
			printf("\nEnd.\n");
			break;
		}
	}

	close(sockfd);

	return EXIT_SUCCESS;
}


void str_trim_lf (char *arr) {
	int len = strlen(arr);
	for (int i = 0; i < len; i++) { // trim \n
		if (arr[i] == '\n' && arr[i + 1] == '\0') {
			arr[i] = '\0';
			break;
		}
	}
}

void catch_ctrl_c_and_exit(int sig) {
    flag = 1;
}

// Erase line text from terminal
void erase_line_text() {
	printf("\33[2K\r");
}

void send_msg_handler() {
  	char message[LENGTH] = { 0 };
	char buffer[LENGTH + 35] = { 0 }; // = message + name of user + 3 (for 3 time \0)

	while(1) {
		printf("> You: ");
		fgets(message, LENGTH, stdin);
		str_trim_lf(message);

		if (strcmp(message, "exit") == 0) {
				break;
		} else {
		sprintf(buffer, "%s: %s \n", name, message);
		send(sockfd, buffer, strlen(buffer), 0);
		}

		bzero(message, sizeof(message));
		bzero(buffer, sizeof(buffer));
	}
	catch_ctrl_c_and_exit(2);
}

void recv_msg_handler() {
	char message[LENGTH] = {};
  while (1) {
		int receive = recv(sockfd, message, LENGTH, 0);
    if (receive > 0) {
		erase_line_text(); 
		printf("> %s", message);
		printf("> You: ");
		fflush(stdout);
    } else if (receive == 0) {
			break;
    } else {
			// -1
	}
	memset(message, 0, sizeof(message));
  }
}