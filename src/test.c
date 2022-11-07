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

int main(int argc, char* argv[]) {
    bzero(with.with_room, sizeof(with.with_room));

    printf("Input friend id: ");
    scanf("%d%*c", &with.with_id);

    printf("ID: %d\n", with.with_id);

    prompt_input_ver2("Input room name: ", with.with_room);
    printf("Name: %s\n", with.with_room); 
}