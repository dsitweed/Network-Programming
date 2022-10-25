CC := gcc

INC_DIR := ./include

SRC_DIRS := ./src

BUILD_DIR := ./build

CFLAGS := -g -Wall -I$(INC_DIR) -pthread

CLIENT := $(BUILD_DIR)/client

SERVER := $(BUILD_DIR)/server

TEST   := $(BUILD_DIR)/test

CLIENT_SRCS := $(shell find $(SRC_DIRS) -name '*.c' -not -name 'server.c' -not -name 'test.c')

CLIENT_OBJS := $(CLIENT_SRCS:%=$(BUILD_DIR)/%.o)

SERVER_SRCS := $(shell find $(SRC_DIRS) -name '*.c' -not -name 'client.c' -not -name 'test.c')

SERVER_OBJS := $(SERVER_SRCS:%=$(BUILD_DIR)/%.o)

TEST_SRCS := $(shell find $(SRC_DIRS) -name '*.c' -not -name 'client.c' -not -name 'server.c')

TEST_OBJS := $(TEST_SRCS:%=$(BUILD_DIR)/%.o)

all: $(CLIENT) $(SERVER) $(TEST)

$(CLIENT): $(CLIENT_OBJS)
	$(CC) $(CFLAGS) $(CLIENT_OBJS) -o $@ lib/libfdr.a
	
$(SERVER): $(SERVER_OBJS)
	$(CC) $(CFLAGS) $(SERVER_OBJS) -o $@ lib/libfdr.a	

$(TEST): $(TEST_OBJS)
	$(CC) $(CFLAGS) $(TEST_OBJS) -o $@ lib/libfdr.a	

$(BUILD_DIR)/%.c.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

server: $(SERVER)
	$(SERVER) 6000

client: $(CLIENT)
	$(CLIENT) 6000

test: $(TEST)
	$(TEST)

clean:
	rm -r $(BUILD_DIR)