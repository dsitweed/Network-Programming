#ifndef UTILS_H
#define UTILS_H

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <termios.h>
#include <time.h>

#define BUFF_SIZE 1024
#define CLIENT_NAME_LEN 32

#define PRINT_ERROR printf("[Error at line %d in %s]: %s\n", __LINE__, __FILE__, strerror(errno))

typedef struct client {
    struct sockaddr_in addr;
    int sockfd;
    int id;
    char username[CLIENT_NAME_LEN];
} * Client;  // client is poiter

typedef struct account_ {
    char username[CLIENT_NAME_LEN];
    char password[CLIENT_NAME_LEN];
    int accStatus;
} Account;

int prompt_input(char const *message, char *buff);

void str_trim_lf(char *arr);

void append(char **p_src, char *dest);

void clear_line();

Client make_client(/* struct sockaddr_in addr, */ int fd, int id, char *username);

/*
    Cần xem xét lại về tính thực tế của hàm này
*/
int config_server(int *fd, struct sockaddr_in *addr);

#endif  // UTILS_H