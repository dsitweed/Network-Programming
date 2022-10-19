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
    if (link->head != NULL) {
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

#endif // end queue