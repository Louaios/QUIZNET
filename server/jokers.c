#include "jokers.h"
#include "questions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>

char* apply_joker(Session *s, int playerIndex, const char *jokerType) {
    if (!s || playerIndex < 0 || playerIndex >= s->playerCount) {
        return NULL;
    }
    
    Player *p = &s->players[playerIndex];
    
    if (strcmp(jokerType, "fifty") == 0) {
        if (p->jokersUsed[JOKER_5050]) {
            return strdup("{\"action\":\"joker/used\",\"statut\":\"400\",\"message\":\"joker already used\"}");
        }
        return apply_5050(s, playerIndex);
    }
    else if (strcmp(jokerType, "skip") == 0) {
        if (p->jokersUsed[JOKER_SKIP]) {
            return strdup("{\"action\":\"joker/used\",\"statut\":\"400\",\"message\":\"joker already used\"}");
        }
        return apply_skip(s, playerIndex);
    }
    
    return NULL;
}

char* apply_5050(Session *s, int playerIndex) {
    Player *p = &s->players[playerIndex];
    p->jokersUsed[JOKER_5050] = 1;
    
    Question *q = &s->questions[s->currentQuestionIndex];
    
    if (strlen(q->answers[1]) == 0) {
        return strdup("{\"action\":\"joker/used\",\"statut\":\"400\",\"message\":\"cannot use 50/50 on this question\"}");
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "joker/used");
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddStringToObject(response, "type", "fifty");
    
    cJSON *removed = cJSON_CreateArray();
    
    int answers_count = 0;
    for (int i = 0; i < MAX_ANSWERS && strlen(q->answers[i]) > 0; i++) {
        answers_count++;
    }
    
    srand(time(NULL) + playerIndex);
    
    int removed_count = 0;
    int target_remove = answers_count / 2;
    
    for (int i = 0; i < MAX_ANSWERS && removed_count < target_remove; i++) {
        if (i != q->correct_index && strlen(q->answers[i]) > 0) {
            cJSON_AddItemToArray(removed, cJSON_CreateNumber(i));
            removed_count++;
        }
    }
    
    cJSON_AddItemToObject(response, "removedAnswers", removed);
    
    char *result = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return result;
}

char* apply_skip(Session *s, int playerIndex) {
    /*Player *p = &s->players[playerIndex];
    p->jokersUsed[JOKER_SKIP] = 1;
        
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "joker/used");
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddStringToObject(response, "type", "skip");
    cJSON_AddStringToObject(response, "message", "question skipped");
    
    char *result = cJSON_PrintUnformatted(response);
    cJSON_Delete(response);
    
    return result;*/
    return NULL;
}
