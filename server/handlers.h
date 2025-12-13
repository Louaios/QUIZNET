#ifndef HANDLERS_H
#define HANDLERS_H

#include "sessions.h"
#include <cJSON.h>

// Variables globales partagées avec server.c
extern Session global_sessions[MAX_SESSIONS];
extern int session_counter;
extern pthread_mutex_t sessions_lock;
extern cJSON *global_users;

// Handlers pour chaque action du protocole
cJSON* handle_register(cJSON *request);
cJSON* handle_login(cJSON *request, char *username);
cJSON* handle_sessions_list(void);
cJSON* handle_session_create(cJSON *request, char *username, int sock, int *session_id);
cJSON* handle_session_join(cJSON *request, char *username, int sock, int *session_id);
int handle_session_start(int session_id);
int handle_question_answer(cJSON *request, int session_id, int sock);
int handle_joker_use(cJSON *request, int session_id, int sock);
int handle_chat_send(cJSON *request, int session_id, int sock);

// Fonction utilitaire pour envoyer du JSON
void send_json_to_socket(int sock, cJSON *json);

// Envoyer les résultats d'une question
void send_question_results(Session *s, int correct_answer_index);

#endif
