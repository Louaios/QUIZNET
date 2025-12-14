#include "handlers.h"
#include "users.h"
#include "utils.h"
#include "questions.h"
#include "jokers.h"
#include <strings.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>


// ============================================================================
// FONCTION UTILITAIRE : Envoi de JSON vers un socket
// ============================================================================
void send_json_to_socket(int sock, cJSON *json) {
    char *str = cJSON_PrintUnformatted(json);
    send(sock, str, strlen(str), 0);
    send(sock, "\n", 1, 0);
    free(str);
}


// ============================================================================
// HANDLER : Inscription d'un nouveau joueur
// ============================================================================
cJSON* handle_register(cJSON *request) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "player/register");
    
    cJSON *pseudo = cJSON_GetObjectItem(request, "pseudo");
    cJSON *password = cJSON_GetObjectItem(request, "password");
    
    if (pseudo && password) {
        if (add_user(global_users, pseudo->valuestring, password->valuestring)) {
            cJSON_AddStringToObject(response, "statut", "201");
            cJSON_AddStringToObject(response, "message", "player registered successfully");
        } else {
            cJSON_AddStringToObject(response, "statut", "409");
            cJSON_AddStringToObject(response, "message", "pseudo already exists");
        }
    } else {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "missing fields");
    }
    
    return response;
}


// ============================================================================
// HANDLER : Connexion d'un joueur
// ============================================================================
cJSON* handle_login(cJSON *request, char *username) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "player/login");
    
    cJSON *pseudo = cJSON_GetObjectItem(request, "pseudo");
    cJSON *password = cJSON_GetObjectItem(request, "password");
    
    if (pseudo && password) {
        if (check_login(global_users, pseudo->valuestring, password->valuestring)) {
            strncpy(username, pseudo->valuestring, MAX_PLAYER_NAME - 1);
            cJSON_AddStringToObject(response, "statut", "200");
            cJSON_AddStringToObject(response, "message", "login successful");
        } else {
            cJSON_AddStringToObject(response, "statut", "401");
            cJSON_AddStringToObject(response, "message", "invalid credentials");
        }
    } else {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "missing fields");
    }
    
    return response;
}


// ============================================================================
// HANDLER : Liste des sessions disponibles
// ============================================================================
cJSON* handle_sessions_list(void) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "sessions/list");
    
    cJSON *sessions_array = cJSON_CreateArray();
    
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId >= 0 && !global_sessions[i].isStarted) {
            cJSON *sess = cJSON_CreateObject();
            cJSON_AddNumberToObject(sess, "id", global_sessions[i].sessionId);
            cJSON_AddStringToObject(sess, "name", global_sessions[i].name);
            cJSON_AddStringToObject(sess, "mode", global_sessions[i].mode);
            cJSON_AddStringToObject(sess, "status", "waiting");
            cJSON_AddNumberToObject(sess, "nbPlayers", global_sessions[i].playerCount);
            cJSON_AddNumberToObject(sess, "maxPlayers", MAX_PLAYERS);
            cJSON_AddNumberToObject(sess, "nbQuestions", global_sessions[i].nbQuestions);
            
            const char *diff[] = {"facile", "moyen", "difficile"};
            cJSON_AddStringToObject(sess, "difficulty", diff[global_sessions[i].difficulty]);
            
            cJSON_AddItemToArray(sessions_array, sess);
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddItemToObject(response, "sessions", sessions_array);
    
    return response;
}


// ============================================================================
// HANDLER : Création d'une nouvelle session
// ============================================================================
cJSON* handle_session_create(cJSON *request, char *username, int sock, int *session_id) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "session/create");
    
    cJSON *name = cJSON_GetObjectItem(request, "name");
    cJSON *mode = cJSON_GetObjectItem(request, "mode");
    cJSON *nbQuestions = cJSON_GetObjectItem(request, "nbQuestions");
    cJSON *timeLimit = cJSON_GetObjectItem(request, "timeLimit");
    cJSON *difficulty_str = cJSON_GetObjectItem(request, "difficulty");
    
    if (name && mode && nbQuestions && timeLimit && difficulty_str) {
        pthread_mutex_lock(&sessions_lock);
        
        Session *new_session = NULL;
        for (int i = 0; i < MAX_SESSIONS; i++) {
            if (global_sessions[i].sessionId < 0) {
                new_session = &global_sessions[i];
                break;
            }
        }
        
        if (new_session) {
            memset(new_session, 0, sizeof(Session));
            new_session->sessionId = session_counter++;
            strncpy(new_session->name, name->valuestring, MAX_SESSION_NAME - 1);
            strncpy(new_session->mode, mode->valuestring, 15);
            new_session->nbQuestions = nbQuestions->valueint;
            new_session->timeLimit = timeLimit->valueint;
            
            const char *diff_val = difficulty_str->valuestring;
            if (strcmp(diff_val, "facile") == 0) new_session->difficulty = 0;
            else if (strcmp(diff_val, "moyen") == 0) new_session->difficulty = 1;
            else new_session->difficulty = 2;
            
            new_session->isStarted = 0;
            new_session->playerCount = 0;
            pthread_mutex_init(&new_session->lock, NULL);
            
            int selected = select_questions(new_session->questions, 
                                           new_session->nbQuestions, 
                                           new_session->difficulty, 
                                           "all");
            
            if (selected > 0) {
                add_player_to_session(new_session, username, sock);
                *session_id = new_session->sessionId;
                
                if (strcmp(new_session->mode, "battle") == 0) {
                    new_session->players[0].lives = 4;
                    new_session->players[0].isEliminated = 0;
                }
                
                log_info("Session created with ID=%d", *session_id);
                
                cJSON_AddStringToObject(response, "statut", "201");
                cJSON_AddStringToObject(response, "message", "session created");
                cJSON_AddNumberToObject(response, "sessionId", new_session->sessionId);
                cJSON_AddStringToObject(response, "mode", new_session->mode);
                cJSON_AddBoolToObject(response, "isCreator", 1);
                
                if (strcmp(new_session->mode, "battle") == 0) {
                    cJSON_AddNumberToObject(response, "lives", 4);
                }
                
                cJSON *jokers = cJSON_CreateObject();
                cJSON_AddNumberToObject(jokers, "fifty", 1);
                cJSON_AddNumberToObject(jokers, "skip", 1);
                cJSON_AddItemToObject(response, "jokers", jokers);
                
                cJSON *players_array = cJSON_CreateArray();
                cJSON_AddItemToArray(players_array, cJSON_CreateString(username));
                cJSON_AddItemToObject(response, "players", players_array);
            } else {
                new_session->sessionId = -1;
                cJSON_AddStringToObject(response, "statut", "500");
                cJSON_AddStringToObject(response, "message", "not enough questions");
            }
        } else {
            cJSON_AddStringToObject(response, "statut", "503");
            cJSON_AddStringToObject(response, "message", "server full");
        }
        
        pthread_mutex_unlock(&sessions_lock);
    } else {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "missing fields");
    }
    
    return response;
}


// ============================================================================
// HANDLER : Rejoindre une session existante
// ============================================================================
cJSON* handle_session_join(cJSON *request, char *username, int sock, int *session_id) {
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "action", "session/join");
    
    cJSON *session_id_json = cJSON_GetObjectItem(request, "sessionId");
    if (!session_id_json) {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "missing sessionId");
        return response;
    }
    
    int target_session_id = session_id_json->valueint;
    Session *target_session = NULL;
    
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId == target_session_id) {
            target_session = &global_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    
    if (!target_session) {
        cJSON_AddStringToObject(response, "statut", "404");
        cJSON_AddStringToObject(response, "message", "session not found");
        return response;
    }
    
    if (target_session->isStarted) {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "session already started");
        return response;
    }
    
    if (target_session->playerCount >= MAX_PLAYERS) {
        cJSON_AddStringToObject(response, "statut", "400");
        cJSON_AddStringToObject(response, "message", "session is full");
        return response;
    }
    
    add_player_to_session(target_session, username, sock);
    *session_id = target_session_id;
    
    if (strcmp(target_session->mode, "battle") == 0) {
        target_session->players[target_session->playerCount - 1].lives = 4;
        target_session->players[target_session->playerCount - 1].isEliminated = 0;
    }
    
    log_info("Player %s joined session %d (%d/%d players)", 
            username, target_session_id, 
            target_session->playerCount, MAX_PLAYERS);
    
    cJSON_AddStringToObject(response, "statut", "200");
    cJSON_AddStringToObject(response, "message", "joined session");
    cJSON_AddNumberToObject(response, "sessionId", target_session->sessionId);
    cJSON_AddStringToObject(response, "mode", target_session->mode);
    cJSON_AddBoolToObject(response, "isCreator", 0);
    
    if (strcmp(target_session->mode, "battle") == 0) {
        cJSON_AddNumberToObject(response, "lives", 4);
    }
    
    cJSON *jokers = cJSON_CreateObject();
    cJSON_AddNumberToObject(jokers, "fifty", 1);
    cJSON_AddNumberToObject(jokers, "skip", 1);
    cJSON_AddItemToObject(response, "jokers", jokers);
    
    cJSON *players_array = cJSON_CreateArray();
    for (int i = 0; i < target_session->playerCount; i++) {
        cJSON_AddItemToArray(players_array, cJSON_CreateString(target_session->players[i].pseudo));
    }
    cJSON_AddItemToObject(response, "players", players_array);
    
    cJSON *notification = cJSON_CreateObject();
    cJSON_AddStringToObject(notification, "action", "session/player-joined");
    cJSON_AddStringToObject(notification, "pseudo", username);
    cJSON_AddNumberToObject(notification, "playerCount", target_session->playerCount);
    
    for (int i = 0; i < target_session->playerCount - 1; i++) {
        send_json_to_socket(target_session->players[i].sock, notification);
    }
    cJSON_Delete(notification);
    
    return response;
}


// Déclaration forward
void send_next_question(Session *s, int playerIndex);


// ============================================================================
// HANDLER : Démarrage d'une session
// ============================================================================
int handle_session_start(int session_id) {
    log_info("Session/start received - session_id=%d", session_id);
    
    Session *current_session = NULL;
    
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId == session_id) {
            current_session = &global_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    
    if (!current_session || current_session->isStarted) {
        return 0;
    }
    
    if (strcmp(current_session->mode, "battle") == 0 && current_session->playerCount < 2) {
        log_error("Cannot start battle mode with less than 2 players");
        return 0;
    }
    
    if (current_session->playerCount < 1) {
        return 0;
    }
    
    current_session->isStarted = 1;
    
    cJSON *start_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(start_msg, "action", "session/started");
    cJSON_AddStringToObject(start_msg, "statut", "200");
    cJSON_AddNumberToObject(start_msg, "countdown", 3);
    
    log_info("Sending session/started to %d players", current_session->playerCount);
    
    for (int i = 0; i < current_session->playerCount; i++) {
        send_json_to_socket(current_session->players[i].sock, start_msg);
    }
    cJSON_Delete(start_msg);
    
    log_info("Sending first question to each player");
    for (int i = 0; i < current_session->playerCount; i++) {
        send_next_question(current_session, i);
    }
    
    return 1;
}


// ============================================================================
// FONCTION : Envoyer les résultats d'une question
// ============================================================================
void send_question_results(Session *s, int correct_answer_index) {
    cJSON *results_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(results_msg, "action", "question/results");
    cJSON_AddNumberToObject(results_msg, "correctAnswer", correct_answer_index);
    
    Player sorted_players[MAX_PLAYERS];
    memcpy(sorted_players, s->players, s->playerCount * sizeof(Player));
    
    for (int i = 0; i < s->playerCount - 1; i++) {
        for (int j = 0; j < s->playerCount - i - 1; j++) {
            if (sorted_players[j].score < sorted_players[j + 1].score) {
                Player temp = sorted_players[j];
                sorted_players[j] = sorted_players[j + 1];
                sorted_players[j + 1] = temp;
            }
        }
    }
    
    cJSON *ranking = cJSON_CreateArray();
    for (int i = 0; i < s->playerCount; i++) {
        if (strcmp(s->mode, "battle") == 0 && sorted_players[i].isEliminated) {
            continue;
        }
        
        cJSON *player_rank = cJSON_CreateObject();
        cJSON_AddNumberToObject(player_rank, "rank", i + 1);
        cJSON_AddStringToObject(player_rank, "pseudo", sorted_players[i].pseudo);
        cJSON_AddNumberToObject(player_rank, "score", sorted_players[i].score);
        
        if (strcmp(s->mode, "battle") == 0) {
            cJSON_AddNumberToObject(player_rank, "lives", sorted_players[i].lives);
        }
        
        cJSON_AddItemToArray(ranking, player_rank);
    }
    cJSON_AddItemToObject(results_msg, "ranking", ranking);
    
    for (int i = 0; i < s->playerCount; i++) {
        send_json_to_socket(s->players[i].sock, results_msg);
    }
    
    cJSON_Delete(results_msg);
    log_info("Sent question results to %d players", s->playerCount);
}


// ============================================================================
// HANDLER : Réponse à une question
// 🔧 CORRECTION : sleep retiré, vérifications renforcées
// ============================================================================
int handle_question_answer(cJSON *request, int session_id, int sock) {
    Session *current_session = NULL;
    
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId == session_id) {
            current_session = &global_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    
    if (!current_session || !current_session->isStarted) {
        return 0;
    }
    
    cJSON *answer_json = cJSON_GetObjectItem(request, "answer");
    if (!answer_json) {
        return 0;
    }
    
    int player_idx = -1;
    for (int i = 0; i < current_session->playerCount; i++) {
        if (current_session->players[i].sock == sock) {
            player_idx = i;
            break;
        }
    }
    if (player_idx == -1) return 0;

    int player_q_index = current_session->players[player_idx].currentQuestionIndex;
    if (player_q_index < 0 || player_q_index >= current_session->nbQuestions) {
        log_error("Invalid player question index: %d for player %s", player_q_index,
                  current_session->players[player_idx].pseudo);
        return 0;
    }

    Question *current_q = &current_session->questions[player_q_index];
    int is_correct = 0;
    
    if (cJSON_IsNumber(answer_json)) {
        int answer_idx = answer_json->valueint;
        is_correct = (answer_idx == current_q->correct_index);
    } else if (cJSON_IsString(answer_json)) {
        const char *answer_str = answer_json->valuestring;
        
        if (strcmp(current_q->answers[0], "true") == 0 || 
            strcmp(current_q->answers[0], "false") == 0) {
            is_correct = (strcmp(answer_str, current_q->answers[current_q->correct_index]) == 0);
        } else {
            for (int i = 0; i < MAX_ANSWERS && strlen(current_q->answers[i]) > 0; i++) {
                if (strcasecmp(answer_str, current_q->answers[i]) == 0) {
                    is_correct = 1;
                    break;
                }
            }
        }
    }
    
    int base_points[] = {5, 10, 15};
    int bonus_points[] = {1, 3, 6};
    int points = 0;
    
    
    
    if (strcmp(current_session->mode, "battle") == 0) {
        if (current_session->players[player_idx].isEliminated) {
            return 1;
        }
        
        if (is_correct) {
            points = base_points[current_q->difficulty];
        } else {
            current_session->players[player_idx].lives--;
            
            log_info("Player %s lost a life (%d remaining)", 
                    current_session->players[player_idx].pseudo,
                    current_session->players[player_idx].lives);
            
            if (current_session->players[player_idx].lives <= 0) {
                current_session->players[player_idx].isEliminated = 1;
                
                cJSON *elim_msg = cJSON_CreateObject();
                cJSON_AddStringToObject(elim_msg, "action", "session/player-eliminated");
                cJSON_AddStringToObject(elim_msg, "pseudo", current_session->players[player_idx].pseudo);
                
                for (int i = 0; i < current_session->playerCount; i++) {
                    send_json_to_socket(current_session->players[i].sock, elim_msg);
                }
                cJSON_Delete(elim_msg);
                
                log_info("Player %s has been eliminated!", 
                        current_session->players[player_idx].pseudo);
            }
        }
    } else {
        if (is_correct) {
            points = base_points[current_q->difficulty];
            
            cJSON *response_time_json = cJSON_GetObjectItem(request, "responseTime");
            if (response_time_json && cJSON_IsNumber(response_time_json)) {
                double resp_time = response_time_json->valuedouble;
                double half_time = (double)current_session->timeLimit / 2.0;
                
                if (resp_time < half_time) {
                    points += bonus_points[current_q->difficulty];
                    log_info("Bonus rapidité ! Réponse en %.2fs (< %.2fs)", resp_time, half_time);
                }
            }
        }
    }
    
        current_session->players[player_idx].score += points;
    log_info("Player %s scored %d points (total: %d)", 
            current_session->players[player_idx].pseudo, 
            points, 
            current_session->players[player_idx].score);
    
    send_question_results(current_session, current_q->correct_index);
    
    // 🔧 CORRECTION : sleep retiré (cause de latence)
    // sleep(3);  <- ENLEVÉ
    
    /* advance only this player's question index */
    current_session->players[player_idx].currentQuestionIndex++;
    
    if (strcmp(current_session->mode, "battle") == 0) {
        int remaining = 0;
        for (int i = 0; i < current_session->playerCount; i++) {
            if (!current_session->players[i].isEliminated) {
                remaining++;
            }
        }
        
        if (remaining <= 1) {
            log_info("Battle finished - winner found!");
            goto send_final_results;
        }
    }
    
    /* If all players finished their questions, end the session */
    int all_finished = 1;
    for (int i = 0; i < current_session->playerCount; i++) {
        if (!current_session->players[i].isEliminated &&
            current_session->players[i].currentQuestionIndex < current_session->nbQuestions) {
            all_finished = 0;
            break;
        }
    }

    if (all_finished) {
        goto send_final_results;
    }

    /* Send next question only to this player if they still have questions */
    if (current_session->players[player_idx].currentQuestionIndex < current_session->nbQuestions) {
        log_info("Sending next question to player %s", current_session->players[player_idx].pseudo);
        send_next_question(current_session, player_idx);
        return 1;
    }
    
send_final_results:
    log_info("Game finished - sending results");
    
    cJSON *final_msg = cJSON_CreateObject();
    cJSON_AddStringToObject(final_msg, "action", "session/finished");
    cJSON_AddStringToObject(final_msg, "statut", "200");
    
    Player sorted_players[MAX_PLAYERS];
    memcpy(sorted_players, current_session->players, current_session->playerCount * sizeof(Player));
    
    for (int i = 0; i < current_session->playerCount - 1; i++) {
        for (int j = 0; j < current_session->playerCount - i - 1; j++) {
            if (sorted_players[j].score < sorted_players[j + 1].score) {
                Player temp = sorted_players[j];
                sorted_players[j] = sorted_players[j + 1];
                sorted_players[j + 1] = temp;
            }
        }
    }
    
    cJSON *ranking = cJSON_CreateArray();
    for (int i = 0; i < current_session->playerCount; i++) {
        cJSON *player_result = cJSON_CreateObject();
        cJSON_AddNumberToObject(player_result, "rank", i + 1);
        cJSON_AddStringToObject(player_result, "pseudo", sorted_players[i].pseudo);
        cJSON_AddNumberToObject(player_result, "score", sorted_players[i].score);
        
        if (strcmp(current_session->mode, "battle") == 0) {
            cJSON_AddNumberToObject(player_result, "lives", sorted_players[i].lives);
            cJSON_AddBoolToObject(player_result, "eliminated", sorted_players[i].isEliminated);
        }
        
        cJSON_AddItemToArray(ranking, player_result);
    }
    cJSON_AddItemToObject(final_msg, "ranking", ranking);
    cJSON_AddStringToObject(final_msg, "mode", current_session->mode);
    
    for (int i = 0; i < current_session->playerCount; i++) {
        send_json_to_socket(current_session->players[i].sock, final_msg);
    }
    cJSON_Delete(final_msg);
    
    current_session->sessionId = -1;
    current_session->isStarted = 0;
    
    return 1;
}


// ============================================================================
// HANDLER : Utilisation d'un joker
// 🔧 CORRECTION : ajout gestion du skip qui envoie la question suivante
// ============================================================================
int handle_joker_use(cJSON *request, int session_id, int sock) {
    cJSON *joker_type_json = cJSON_GetObjectItem(request, "type");
    if (!joker_type_json || !cJSON_IsString(joker_type_json)) {
        return 0;
    }
    
    const char *joker_type = joker_type_json->valuestring;
    
    Session *current_session = NULL;
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId == session_id) {
            current_session = &global_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&sessions_lock);
    
    if (!current_session) return 0;
    
    int player_idx = -1;
    for (int i = 0; i < current_session->playerCount; i++) {
        if (current_session->players[i].sock == sock) {
            player_idx = i;
            break;
        }
    }
    
    if (player_idx == -1) return 0;
    
    // TRAITEMENT SKIP
    if (strcmp(joker_type, "skip") == 0) {
        if (current_session->players[player_idx].jokersUsed[JOKER_SKIP]) {
            cJSON *error_response = cJSON_CreateObject();
            cJSON_AddStringToObject(error_response, "action", "joker/used");
            cJSON_AddStringToObject(error_response, "statut", "400");
            cJSON_AddStringToObject(error_response, "message", "joker already used");
            send_json_to_socket(sock, error_response);
            cJSON_Delete(error_response);
            return 0;
        }
        
        current_session->players[player_idx].jokersUsed[JOKER_SKIP] = 1;

        int before = current_session->players[player_idx].currentQuestionIndex;
        log_info("Player %s used skip, current question: %d", 
                current_session->players[player_idx].pseudo,
                before + 1);

        /* advance only this player's index */
        current_session->players[player_idx].currentQuestionIndex++;
        int after = current_session->players[player_idx].currentQuestionIndex;

        log_info("Player %s skipped to question: %d (total: %d)", 
                current_session->players[player_idx].pseudo,
                after + 1,
                current_session->nbQuestions);

        if (after < current_session->nbQuestions) {
            send_next_question(current_session, player_idx);
        } else {
            log_info("Player %s has no more questions after skip", current_session->players[player_idx].pseudo);
        }
        
        return 1;
    }
    
    // TRAITEMENT 50/50
    char *result = apply_joker(current_session, player_idx, joker_type);
    if (!result) return 0;
    
    cJSON *joker_response = cJSON_Parse(result);
    free(result);
    
    if (!joker_response) return 0;
    
    send_json_to_socket(sock, joker_response);
    cJSON_Delete(joker_response);
    
    log_info("Player %s used joker: %s", 
            current_session->players[player_idx].pseudo, joker_type);
    return 1;
}






// ============================================================================
// FONCTION : Envoyer la prochaine question
// ============================================================================
void send_next_question(Session *s, int playerIndex) {
    cJSON *msg = NULL;
    
    if (playerIndex >= 0) {
        if (playerIndex < 0 || playerIndex >= s->playerCount) return;
        int idx = s->players[playerIndex].currentQuestionIndex;
        if (idx >= s->nbQuestions) return;

        Question *q = &s->questions[idx];
        msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "action", "question/new");
        cJSON_AddNumberToObject(msg, "questionNum", idx + 1);
        cJSON_AddNumberToObject(msg, "totalQuestions", s->nbQuestions);
        
        const char *type = "qcm";
        if (strlen(q->answers[1]) == 0 || strcmp(q->answers[0], "true") == 0 || strcmp(q->answers[0], "false") == 0) {
            if (strcmp(q->answers[0], "true") == 0 || strcmp(q->answers[0], "false") == 0) {
                type = "boolean";
            } else {
                type = "text";
            }
        }
        cJSON_AddStringToObject(msg, "type", type);
        if (strcmp(type, "qcm") == 0) {
            cJSON *answers = cJSON_CreateArray();
            for (int i = 0; i < MAX_ANSWERS && strlen(q->answers[i]) > 0; i++) {
                cJSON_AddItemToArray(answers, cJSON_CreateString(q->answers[i]));
            }
            cJSON_AddItemToObject(msg, "answers", answers);
        }
        const char *diff[] = {"facile", "moyen", "difficile"};
        cJSON_AddStringToObject(msg, "difficulty", diff[q->difficulty]);
        cJSON_AddStringToObject(msg, "question", q->question);

        log_info("Sending question %d to player %s: %s", idx + 1, s->players[playerIndex].pseudo, q->question);

        send_json_to_socket(s->players[playerIndex].sock, msg);
        cJSON_Delete(msg);
        return;
    }

    /* playerIndex < 0 : send each player their own next question */
    for (int p = 0; p < s->playerCount; p++) {
        int idx = s->players[p].currentQuestionIndex;
        if (idx >= s->nbQuestions) continue;
        Question *q = &s->questions[idx];
        msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "action", "question/new");
        cJSON_AddNumberToObject(msg, "questionNum", idx + 1);
        cJSON_AddNumberToObject(msg, "totalQuestions", s->nbQuestions);
        
        const char *type = "qcm";
        if (strlen(q->answers[1]) == 0 || strcmp(q->answers[0], "true") == 0 || strcmp(q->answers[0], "false") == 0) {
            if (strcmp(q->answers[0], "true") == 0 || strcmp(q->answers[0], "false") == 0) {
                type = "boolean";
            } else {
                type = "text";
            }
        }
        cJSON_AddStringToObject(msg, "type", type);
        if (strcmp(type, "qcm") == 0) {
            cJSON *answers = cJSON_CreateArray();
            for (int i = 0; i < MAX_ANSWERS && strlen(q->answers[i]) > 0; i++) {
                cJSON_AddItemToArray(answers, cJSON_CreateString(q->answers[i]));
            }
            cJSON_AddItemToObject(msg, "answers", answers);
        }
        const char *diff[] = {"facile", "moyen", "difficile"};
        cJSON_AddStringToObject(msg, "difficulty", diff[q->difficulty]);
        cJSON_AddStringToObject(msg, "question", q->question);

        log_info("Sending question %d to player %s: %s", idx + 1, s->players[p].pseudo, q->question);
        send_json_to_socket(s->players[p].sock, msg);
        cJSON_Delete(msg);
        msg = NULL;
    }
    
}


// ============================================================================
// HANDLER : Envoi d'un message chat dans une session
// ============================================================================
int handle_chat_send(cJSON *request, int session_id, int sock) {
    cJSON *msg_json = cJSON_GetObjectItem(request, "message");
    if (!msg_json || !cJSON_IsString(msg_json)) return 0;

    const char *message = msg_json->valuestring;

    Session *current_session = NULL;
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        if (global_sessions[i].sessionId == session_id) {
            current_session = &global_sessions[i];
            break;
        }
    }
    pthread_mutex_unlock(&sessions_lock);

    if (!current_session) return 0;

    int player_idx = -1;
    for (int i = 0; i < current_session->playerCount; i++) {
        if (current_session->players[i].sock == sock) {
            player_idx = i;
            break;
        }
    }

    if (player_idx == -1) return 0;

    const char *pseudo = current_session->players[player_idx].pseudo;

    // Build broadcast message
    cJSON *broadcast = cJSON_CreateObject();
    cJSON_AddStringToObject(broadcast, "action", "chat/message");
    cJSON_AddStringToObject(broadcast, "pseudo", pseudo);
    cJSON_AddStringToObject(broadcast, "message", message);

    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    char ts[64] = {0};
    if (tm) strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", tm);
    cJSON_AddStringToObject(broadcast, "timestamp", ts);

    // Send to all players in the session
    for (int i = 0; i < current_session->playerCount; i++) {
        send_json_to_socket(current_session->players[i].sock, broadcast);
    }

    cJSON_Delete(broadcast);

    // Send ACK to sender
    cJSON *ack = cJSON_CreateObject();
    cJSON_AddStringToObject(ack, "action", "chat/sent");
    cJSON_AddStringToObject(ack, "statut", "200");
    send_json_to_socket(sock, ack);
    cJSON_Delete(ack);

    log_info("Chat message from %s broadcasted in session %d", pseudo, session_id);
    return 1;
}
