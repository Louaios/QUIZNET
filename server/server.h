#ifndef SERVER_H
#define SERVER_H

#define _POSIX_C_SOURCE 200809L

#include <pthread.h>

#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #endif
#else
  #include <cjson/cJSON.h>
#endif

#include "sessions.h"
#include "questions.h"
#include "jokers.h"
#include "utils.h"

#define TCP_PORT 5556
#define UDP_PORT 5555
#define MAX_BUFFER 1024

void* client_handler(void *arg);
void* udp_broadcast(void *arg);
void send_next_question(Session *s, int playerIndex);

#endif
