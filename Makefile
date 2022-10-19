FLAGS    = -L /lib64
LIBS     = -lusb-1.0 -l pthread

compile:
	gcc -Wall -g3 -fsanitize=address -pthread server.c -g -o server
	gcc -Wall -g3 -fsanitize=address -pthread client.c -g -o client
