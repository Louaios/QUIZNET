CC = gcc
CFLAGS = -Wall -Wextra -pthread -D_POSIX_C_SOURCE=200809L -g
LDFLAGS = -lcjson -lssl -lcrypto -pthread

CJSON_CFLAGS := $(shell pkg-config --cflags libcjson 2>/dev/null)
ifeq ($(CJSON_CFLAGS),)
    ifneq ($(wildcard /usr/include/cjson/cJSON.h),)
        CJSON_CFLAGS = -I/usr/include/cjson
    endif
endif

CFLAGS += $(CJSON_CFLAGS)

SRCS = server.c handlers.c sessions.c questions.c users.c jokers.c utils.c
OBJS = $(SRCS:.c=.o)
TARGET = quiznet_server

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) users.json

run: $(TARGET)
	./$(TARGET)
