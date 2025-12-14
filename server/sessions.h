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

typedef struct {
    char pseudo[MAX_PLAYER_NAME];
    int sock;
    int score;
    int jokersUsed[2];
    int lives;          // Nombre de vies (mode battle)
    int isEliminated;   // 1 si éliminé, 0 sinon
    int currentQuestionIndex; // progression individuelle du joueur
} Player;

typedef struct Session {
    int sessionId;
    char name[MAX_SESSION_NAME];
    char mode[16];
    int nbQuestions;
    int timeLimit;
    int difficulty;
    int playerCount;
    /* session-level currentQuestionIndex removed to allow per-player progression */
    int isStarted;
    Player players[MAX_PLAYERS];
    Question questions[100];
    pthread_mutex_t lock;
} Session;

Session* create_session_full(const char *name, const char *mode,
                             int nbQuestions, int timeLimit, int difficulty,
                             const char *creatorName, int sock);
Session* join_session(int sessionId, const char* pseudo, int sock);
int add_player_to_session(Session *s, const char *pseudo, int sock);
int find_player_index(Session *s, const char *pseudo);

#endif
