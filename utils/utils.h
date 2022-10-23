#include <termios.h>
#include "errno.h"


#ifndef utils
#define utils


#define PRINT_ERROR printf("[Error at line %d in %s]: %s\n", __LINE__, __FILE__, strerror(errno))

/* Client structure */
typedef struct {
	struct sockaddr_in address;
	int sockfd;
	int uid; // id of client
	char name[32];
} client_t;

typedef struct node_ {
    client_t client;
    struct node_ *next;
    struct node_ *prev;
} Node;

typedef struct DoubleLink_ {
    Node *head; // head
    Node *tail; // tails
} DoubleLink;

Node* createNode (client_t cl) {
    Node *newNode = (Node *) malloc (sizeof(Node));
    newNode->client = cl;
    newNode->next = NULL;
    return newNode;
}

/*
    Head -> node -> node -> Tail -> NULL
*/

DoubleLink* createLinkList() {
    DoubleLink* dbLink = (DoubleLink* ) malloc(sizeof(DoubleLink));
    if (dbLink == NULL) return NULL;
    dbLink->head = NULL;
    dbLink->tail = NULL;
    return dbLink;
}

int isEmpty(const DoubleLink* link) {
    if (link->head == NULL)
        return 1;
    else return 0;
}

void insertAtHead(DoubleLink* link, client_t cl) {
    Node* newNode = createNode(cl);
    if (link->head == NULL) {
        link->head = newNode;
        link->tail = newNode;
    } else {
        link->head->prev = newNode;
        newNode->next = link->head;
        link->head = newNode;
    }
}

void deleteClient(DoubleLink *link, int clientId) {
    client_t client;
    Node* run = link->head;
    if (link->head == NULL) return;

    // if link have 1 node
    if (link->head == link->tail) {
        if (run->client.uid == clientId) {
            link->head = NULL;
            link->tail = NULL;
            return;
        } else return;
    }
    // if delete head
    if (run->client.uid == clientId) {
        link->head = run->next;
        link->head->prev = NULL;
        return;
    }
    // if delete rail
    if (link->tail->client.uid == clientId) {
        link->tail = link->tail->prev;
        link->tail->next = NULL;
        return;
    } 

    while (run != NULL) {
        client = run->client;
        if (client.uid == clientId) {
            run->prev->next = run->next;
            run->next->prev = run->prev;
            free(run);
            return;
        }
        run = run->next;
    }
}

void printNode(Node *node) {
    client_t cli = node->client;
    printf("Name: %s\n", cli.name);
    printf("ID: %d\n", cli.uid);
}

void traverse(DoubleLink *link) {
    Node *run = link->head;
    while (run != NULL) {
        printNode(run);
        run = run->next;
    }
}

static struct termios old, current;

/* Initialize new terminal i/o settings */
void initTermios(int echo) {
    tcgetattr(0, &old);         /* grab old terminal i/o settings */
    current = old;              /* make new settings same as old settings */
    current.c_lflag &= ~ICANON; /* disable buffered i/o */
    if (echo) {
        current.c_lflag |= ECHO; /* set echo mode */
    } else {
        current.c_lflag &= ~ECHO; /* set no echo mode */
    }
    tcsetattr(0, TCSANOW, &current); /* use these new terminal i/o settings now */
}

/* Restore old terminal i/o settings */
void resetTermios(void) { tcsetattr(0, TCSANOW, &old); }

/* Read 1 character - echo defines echo mode */
char getch_(int echo) {
    char ch;
    initTermios(echo);
    ch = getchar();
    resetTermios();
    return ch;
}

/* Read 1 character without echo */
char getch(void) { return getch_(0); }
/* Read 1 character with echo */
char getche(void) { return getch_(1); }

#endif // end