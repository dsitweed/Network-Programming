#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jrb.h"
#include "jval.h"
#include "utils.h"

typedef union chatWith {
    char with_room[33];
    int with_id;
} ChatWith;

ChatWith with;

int len_of_number(int number) {
    int count = 0;
    if (number == 0) return 1;
    while (number > 0) {
        number = number / 10;
        count++;
    }
    return count;
}

void send_message_toPVP(char *s) {
    JRB node;
    int destId, lenNumber = 0;
    char buff[BUFF_SIZE] = { 0 };

    sscanf(s, "%d ", &destId);
    lenNumber = len_of_number(destId);
    strcpy(buff, s + lenNumber + 1);

    printf("%d %s \n",destId, buff);
}

int main(int argc, char* argv[]) {
    JRB node = NULL, list = make_jrb();

    jrb_insert_int(list, 1, new_jval_i(1));

    jrb_traverse(node, list) {
        if (jval_i(node->key) == 1) break;
    }

    node->key = new_jval_i(3);

    node = jrb_find_int(list, 3);

    if (node == NULL) {
        printf ("NULL\n");
    } else 
        printf ("NOT NULL %d\n", jval_i(node->key));
}