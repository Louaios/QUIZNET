#include "sessions.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static Session sessions[MAX_SESSIONS];
static int session_id_counter = 0;

Session* create_session_full(const char *name, const char *mode,
                             int nbQuestions, int timeLimit, int difficulty,
                             const char *creatorName, int sock) {
    Session *s = NULL;
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].sessionId == -1 || sessions[i].isStarted == -1) {
            s = &sessions[i];
            break;
        }
    }
    
    if (!s) return NULL;
    
    memset(s, 0, sizeof(Session));
    s->sessionId = session_id_counter++;
    strncpy(s->name, name, MAX_SESSION_NAME - 1);
    strncpy(s->mode, mode, 15);
    s->nbQuestions = nbQuestions;
    s->timeLimit = timeLimit;
    s->difficulty = difficulty;
    s->playerCount = 0;
    s->isStarted = 0;
    pthread_mutex_init(&s->lock, NULL);
    
    add_player_to_session(s, creatorName, sock);
    
    return s;
}

Session* join_session(int sessionId, const char* pseudo, int sock) {
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (sessions[i].sessionId == sessionId && !sessions[i].isStarted) {
            if (add_player_to_session(&sessions[i], pseudo, sock)) {
                return &sessions[i];
            }
        }
    }
    return NULL;
}

int add_player_to_session(Session *s, const char *pseudo, int sock) {
    if (s->playerCount >= MAX_PLAYERS) return 0;
    
    pthread_mutex_lock(&s->lock);
    
    Player *p = &s->players[s->playerCount];
    strncpy(p->pseudo, pseudo, MAX_PLAYER_NAME - 1);
    p->sock = sock;
    p->score = 0;
    p->jokersUsed[0] = 0;
    p->jokersUsed[1] = 0;
    p->lives = 4;           // Initialiser les vies
    p->isEliminated = 0;    // Pas éliminé au départ
    p->currentQuestionIndex = 0; // start at first question for this player
    
    s->playerCount++;
    
    pthread_mutex_unlock(&s->lock);
    return 1;
}

int find_player_index(Session *s, const char *pseudo) {
    for (int i = 0; i < s->playerCount; i++) {
        if (strcmp(s->players[i].pseudo, pseudo) == 0) {
            return i;
        }
    }
    return -1;
}
