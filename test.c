#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>


#include "utils/utils.h"

void readToEndline(char *user_input){
	int user_input_index = 0;
	char c;
	do {
		c = getch();
		user_input[user_input_index++] = c;
	} while (c != '\n');
}

int main(int argc, char *argv[]) {
	client_t client1, client2;
	client1.uid = 1;
	strcpy(client1.name, "123");
	DoubleLink *link = createLinkList();
	insertAtHead(link, client1);
	traverse(link);
	deleteClient(link, 1);
	printf("after delete");
	traverse(link);
}