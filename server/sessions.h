#ifndef SESSIONS_H
#define SESSIONS_H

#include "questions.h"
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

#define MAX_PLAYERS 4
#define MAX_SESSIONS 10
#define MAX_SESSION_NAME 32
#define MAX_PLAYER_NAME 32

#define MAX_CHAT_HISTORY 200
#define MAX_CHAT_MSG_LENGTH 512
#define MAX_TIMESTAMP_LEN 64

typedef struct {
    char pseudo[MAX_PLAYER_NAME];
    int sock;
    int score;
    int jokersUsed[2];
    int lives;          // Nombre de vies (mode battle)
    int isEliminated;   // 1 si éliminé, 0 sinon
} Player;

typedef struct Session {
    int sessionId;
    char name[MAX_SESSION_NAME];
    char mode[16];
    int nbQuestions;
    int timeLimit;
    int difficulty;
    int playerCount;
    int currentQuestionIndex;
    int isStarted;
    Player players[MAX_PLAYERS];
    Question questions[100];
    pthread_mutex_t lock;
  /* Chat history */
  struct {
    char sender[MAX_PLAYER_NAME];
    char message[MAX_CHAT_MSG_LENGTH];
    char timestamp[MAX_TIMESTAMP_LEN];
  } chatHistory[MAX_CHAT_HISTORY];
  int chatCount;
} Session;

Session* create_session_full(const char *name, const char *mode,
                             int nbQuestions, int timeLimit, int difficulty,
                             const char *creatorName, int sock);
Session* join_session(int sessionId, const char* pseudo, int sock);
int add_player_to_session(Session *s, const char *pseudo, int sock);
int find_player_index(Session *s, const char *pseudo);

#endif
