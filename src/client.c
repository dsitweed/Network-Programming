#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>  // signal
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "jrb.h"
#include "jval.h"
#include "protocol.h"
#include "utils.h"

#define RED "\x1B[31m"
#define GRN "\x1B[32m"
#define YEL "\x1B[33m"
#define BLU "\x1B[34m"
#define MAG "\x1B[35m"
#define CYN "\x1B[36m"
#define WHT "\x1B[37m"
#define RESET "\x1B[0m"

#define SUCCESS_MESS "SUCCESS"
#define FAILED_MESS "FAILED"

// global variables

/* An integer type which can be accessed as an atomic
entity even in the presence of asynchronous interrupts made by signals.*/
volatile sig_atomic_t exit_flag = 0;
char clientName[CLIENT_NAME_LEN] = "default_name";  // save client name at global variable
int sockfd = 0;                              // socket of this client

void signal_handler(int sig);
void send_msg_handler();
void recv_msg_handler();

typedef union chatWith {
    char with_room[ROOM_NAME_LEN];
    int with_id;
} ChatWith;

ChatWith with;
int typeChat;

// return 0 error
// return 1 -> ... is success
int auth_menu(int sockfd) {
    int menu, err = 0;
    char buff[BUFF_SIZE] = {0};
    int response;
    Account* acc = (Account*) malloc(sizeof(Account));

    do {
        printf(
            "===== THE CHATROOM AUTH SCREEN ===== \n"
            "1. Register - Sign up\n"
            "2. Sign in \n"
            "3. Exit\n"
            "Your choice (1-3): ");
        scanf("%d%*c", &menu);
        switch (menu) {
            case 1:
                printf("SIGN UP\n");
                prompt_input_ver2("Input username: ", acc->username, sizeof(acc->username));
                prompt_input_ver2("Input password: ", acc->password, sizeof(acc->password));

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", SIGN_UP, acc->username, acc->password);
                sendData(sockfd, buff, strlen(buff));

                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err >= 0) {
                    sscanf(buff, "%d", &response);
                    if (response == SUCCESS) {
                        printf(GRN "Sign up SUCCESS\n" RESET);
                    }
                    if (response == FAILED) {
                        printf(RED "This username is already taken\n" RESET);
                    }
                }
                break;
            case 2:
                printf("SIGN IN\n");
                prompt_input_ver2("Input username: ", acc->username, sizeof(acc->username));
                prompt_input_ver2("Input password: ", acc->password, sizeof(acc->password));

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", SIGN_IN, acc->username, acc->password);
                err = sendData(sockfd, buff, strlen(buff));

                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err >= 0) {
                    sscanf(buff, "%d", &response);
                    if (response == SUCCESS) {
                        strcpy(clientName, acc->username);
                        printf(GRN "Login succeeded\n\n" RESET);
                        return 1;
                    }
                    if (response == FAILED) {
                        printf(RED "Invalid credentials\n\n" RESET);
                    }
                } else
                    printf(RED "Error\n" RESET);
                break;
            case 3:
                printf("\nEXIT\n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s", SIGN_OUT, clientName);
                err = sendData(sockfd, buff, strlen(buff));

                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err >= 0) {
                    sscanf(buff, "%d", &response);
                    if (response == SUCCESS) {
                        printf(GRN "Log out success\n\n" RESET);
                        return EXIT;
                    }

                    if (response == FAILED) {
                        printf(RED "Invalid credentials\n\n" RESET);
                        return 0;
                    }
                } else printf(RED "Error\n" RESET);
                break;
            default:
                printf(RED "Please select valid options\n" RESET);
                break;
        }  // end switch
    } while (menu != 5);
    return 1;
}

int select_room_menu(int sockfd) {
    int menu, err = 0;
    char buff[BUFSIZ] = {0};

    bzero(with.with_room, sizeof(with.with_room));
    /* Restart thread send message and thread receive message */
    exit_flag = 0; 
    do {
        printf(
            "===== THE SELECT ROOM SCREEN ===== \n"
            "1. Create new room\n"
            "2. Show list rooms \n"
            "3. Join room by name \n"
            "4. Show list users \n"
            "5. Chat PvP with 1 user by id \n"
            "6. Sign out of chat room \n"
            "Your choice (1-6): ");
        scanf("%d%*c", &menu);
        switch (menu) {
            case 1:
                {char new_name[ROOM_NAME_LEN] = {0};
                int action = -1;
                printf("Create new room\n");
                prompt_input_ver2("Name of room (not contain spaces): ", new_name, sizeof(new_name));

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s %s", CREATE_NEW_ROOM, new_name, clientName);
                sendData(sockfd, buff, strlen(buff));
                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d", &action);
                    if (action == SUCCESS) printf (GRN "Create room success\n" RESET);
                    if (action == FAILED) printf (RED "Create room failed\n" RESET);
                } else printf (RED "HAVE ERROR\n" RESET);
                break;
                }
            case 2:
                {int action = -1;
                printf("Show list rooms\n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", SHOW_LIST_ROOMS);
                send(sockfd, buff, strlen(buff), 0);

                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d ", &action);
                    strcpy(buff, buff + len_of_number(action) + 1);
                    puts(buff);
                } else printf (RED "HAVE ERROR\n" RESET);
                break;
                }
            case 3:
                {int action = -1;
                char room_name[ROOM_NAME_LEN] = {0};
                printf("Join room by name\n");
                prompt_input_ver2("Input room name: ", room_name, sizeof(with.with_room));
                strcpy(with.with_room, room_name);

                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s", JOIN_ROOM, room_name);
                send(sockfd, buff, strlen(buff), 0);

                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d", &action);
                    if (action == SUCCESS) {
                        printf (GRN "Join room success\n" RESET);
                        return JOIN_ROOM;
                    }
                    if (action == FAILED) printf (RED "Join room failed\n" RESET);
                } else printf (RED "HAVE ERROR\n" RESET);
                break;
            }
            case 4:
                {int action = -1;
                printf("Show list users\n");
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d", SHOW_LIST_USERS);
                send(sockfd, buff, strlen(buff), 0);

                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d ", &action);
                    strcpy(buff, buff + len_of_number(action) + 1);
                    puts(buff);
                } else printf (RED "HAVE ERROR\n" RESET);
                break;
                }
            case 5: 
                {int action = -1;
                printf("Chat PvP with 1 user by id \n");
                bzero(buff, sizeof(buff));
                printf("Input friend id: ");
                scanf("%d%*c", &with.with_id);

                sprintf(buff, "%d %d", CONNECT_PVP, with.with_id);
                send(sockfd, buff, strlen(buff), 0);
                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d", &action);
                    if (action == SUCCESS) {
                        printf (GRN "Connect success\n" RESET);
                        return CONNECT_PVP;
                    }
                    if (action == FAILED) printf (RED "Connect room failed\n" RESET);
                } else printf (RED "HAVE ERROR\n" RESET);
                break;
            }
            case 6:
                {int action = -1;
                bzero(buff, sizeof(buff));
                sprintf(buff, "%d %s", SIGN_OUT, clientName);
                send(sockfd, buff, strlen(buff), 0);
                
                // Received response
                bzero(buff, sizeof(buff));
                err = recvData(sockfd, buff, sizeof(buff));
                if (err > 0) {
                    sscanf(buff, "%d", &action);
                    if (action == SUCCESS) printf (GRN "EXIT success\n" RESET);
                    if (action == FAILED) printf (RED "EXIT failed\n" RESET);
                } else printf (RED "HAVE ERROR\n" RESET);
                
                return EXIT;
                }
            default:
                printf("Please select valid options\n");
                break;
        }  // end switch
    } while (menu != 6);
    // if (action_flag == EXIT) return EXIT;
    // if (action_flag == PVP_CHAT) return CONNECT_PVP;
    // if (action_flag == JOIN_ROOM) return JOIN_ROOM;
    return 0;
}

// return 1 no error
int chat_in_room_menu(int sockfd) {
    pthread_t send_mesg_thread;
    pthread_t recv_mesg_thread;

    printf("===== WELCOME TO THE CHATROOM =====\n");

    if (pthread_create(&send_mesg_thread, NULL, (void *)send_msg_handler, NULL) != 0) {
        printf(RED "ERROR: pthread\n" RESET);
        return EXIT_FAILURE;
    }

    if (pthread_create(&recv_mesg_thread, NULL, (void *)recv_msg_handler, NULL) != 0) {
        printf(RED "ERROR: pthread\n" RESET);
        return EXIT_FAILURE;
    }

    // communicate with server
    while (1) {
        if(exit_flag) {
            pthread_detach(recv_mesg_thread); 
            pthread_detach(send_mesg_thread);
            printf("\nEND CHAT ROOM.\n");
            break;
        }    
    }
    return 1;
}

int main(int argc, char const *argv[]) {
    if (argc != 2) {
        printf(RED "Usage: %s <port>\n" RESET, argv[1]);
        return EXIT_FAILURE;
    }

    int err, main_exit_flag = 0;
    struct sockaddr_in server_addr;
    char *IP = "127.0.0.1";
    int PORT = atoi(argv[1]);

    signal(SIGINT, signal_handler);

    /*Socket settings*/
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(IP);
    server_addr.sin_port = htons(PORT);

    /*Connect to server*/
    err = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (err == -1) {
        printf(RED "ERROR: connect\n" RESET);
        close(sockfd);
        return EXIT_FAILURE;
    }

    /* Check login - Auth_screen */
    err = auth_menu(sockfd);
    if (err == EXIT) main_exit_flag = 1;

    while (1) {
        if (main_exit_flag == 1) break;
        /* Select room  */
        typeChat = select_room_menu(sockfd);
        if (typeChat == EXIT || typeChat == 0) break;

        /* Chat PvP or Chat with Room */
        err = chat_in_room_menu(sockfd);
    }

    close(sockfd);  // have to close socket
    return EXIT_SUCCESS;
}

void signal_handler(int sig) {
    exit_flag = 1;
    exit(0);
}

void send_msg_handler() {
    char message[BUFF_SIZE] = {0};
    char buffer[BUFF_SIZE * 2] = {0};  // = message + name of user + 3 (for 3 time \0)

    while (1) {
        printf("> You: ");
        fgets(message, sizeof(message), stdin);
        int index = strlen(message) - 1;

        if (message[index] == '\n' && message[index + 1] == '\0') {
            message[index] = '\0';
        }

        if (strcasecmp(message, "exit") == 0) {
            sprintf(buffer, "%d", OUT_CHAT);
            sendData(sockfd,buffer, strlen(buffer));
            exit_flag = 1;
            break;
        } else if (typeChat == CONNECT_PVP) {
            sprintf(buffer, "%d %d %s: %s", PVP_CHAT, with.with_id ,clientName, message);
            sendData(sockfd, buffer, strlen(buffer));
        } else if (typeChat == JOIN_ROOM) {
            sprintf(buffer, "%d %s %s: %s", ROOM_CHAT, with.with_room, clientName, message);
            sendData(sockfd, buffer, strlen(buffer));
        }  // end send message

        fflush(stdout);
        bzero(message, sizeof(message));
        bzero(buffer, sizeof(buffer));
    }
}

void recv_msg_handler() {
    char message[BUFF_SIZE] = {0};
    int received = 0;

    while (1) {
        int action_flag = -1;
        received = recvData(sockfd, message, sizeof(message));
        if (received <= 0) {// have error
            break;
        }

        sscanf(message, "%d", &action_flag);
        if (action_flag == EXIT) {
            break;
        }
        if (action_flag == FAILED) {// have error
            clear_line();
            printf("> %s\n", "HAVE ERROR\n");
            break;
        }
        if (action_flag == SUCCESS) continue;

        clear_line();
        printf("> %s\n", message);
        printf("> You: ");

        fflush(stdout);
        memset(message, 0, sizeof(message));
    }
}