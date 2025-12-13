#define _POSIX_C_SOURCE 200809L

#include "server.h"
#include "handlers.h"
#include "users.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define SERVER_NAME "QuizNet_Server_1"

// ============================================================================
// VARIABLES GLOBALES
// ============================================================================
cJSON *global_users = NULL;
Session global_sessions[MAX_SESSIONS];
int session_counter = 0;
pthread_mutex_t sessions_lock = PTHREAD_MUTEX_INITIALIZER;

// ============================================================================
// THREAD UDP : Découverte des serveurs par broadcast
// ============================================================================
void* udp_broadcast(void *arg) {
    (void)arg;
    
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("UDP socket");
        return NULL;
    }
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(UDP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("UDP bind");
        close(sock);
        return NULL;
    }
    
    log_info("UDP broadcast listening on port %d", UDP_PORT);
    
    char buffer[MAX_BUFFER];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    while (1) {
        int n = recvfrom(sock, buffer, MAX_BUFFER - 1, 0,
                        (struct sockaddr*)&client_addr, &client_len);
        
        if (n > 0) {
            buffer[n] = '\0';
            
            if (strcmp(trim(buffer), "looking for quiznet servers") == 0) {
                char response[256];
                snprintf(response, sizeof(response), 
                        "hello i'm a quiznet server:%s:%d", 
                        SERVER_NAME, TCP_PORT);
                
                sendto(sock, response, strlen(response), 0,
                      (struct sockaddr*)&client_addr, client_len);
                
                log_info("Responded to UDP discovery from %s",
                        inet_ntoa(client_addr.sin_addr));
            }
        }
    }
    
    close(sock);
    return NULL;
}

// ============================================================================
// THREAD CLIENT : Gestion d'une connexion client
// ============================================================================
void* client_handler(void *arg) {
    int sock = *(int*)arg;
    free(arg);
    
    char buffer[MAX_BUFFER];
    char username[MAX_PLAYER_NAME] = {0};
    int current_session_id = -1;
    
    while (1) {
        memset(buffer, 0, MAX_BUFFER);
        int n = recv(sock, buffer, MAX_BUFFER - 1, 0);
        if (n <= 0) break;
        
        buffer[n] = '\0';
        log_info("Received: %s", buffer);
        
        char *line_end = strchr(buffer, '\n');
        if (!line_end) {
            char *err = create_error_response(NULL, "400", "Bad request format");
            send(sock, err, strlen(err), 0);
            send(sock, "\n", 1, 0);
            free(err);
            continue;
        }
        
        *line_end = '\0';
        char *json_part = line_end + 1;
        
        char method[16], action[64];
        if (sscanf(buffer, "%s %s", method, action) != 2) {
            char *err = create_error_response(NULL, "400", "Bad request");
            send(sock, err, strlen(err), 0);
            send(sock, "\n", 1, 0);
            free(err);
            continue;
        }
        
        cJSON *request = NULL;
        if (strlen(json_part) > 0) {
            request = cJSON_Parse(json_part);
        }
        
        cJSON *response = NULL;
        int no_response = 0;
        
        // ========================================================================
        // ROUTEUR : Diriger vers le bon handler selon l'action
        // ========================================================================
        
        if (strcmp(action, "player/register") == 0) {
            response = handle_register(request);
        }
        else if (strcmp(action, "player/login") == 0) {
            response = handle_login(request, username);
        }
        else if (strcmp(action, "sessions/list") == 0) {
            response = handle_sessions_list();
        }
        else if (strcmp(action, "session/create") == 0) {
            response = handle_session_create(request, username, sock, &current_session_id);
        }
        else if (strcmp(action, "session/join") == 0) {
            // ✅ CORRECTION : Route session/join ajoutée
            response = handle_session_join(request, username, sock, &current_session_id);
        }
        else if (strcmp(action, "session/start") == 0) {
            if (handle_session_start(current_session_id)) {
                no_response = 1;
            } else {
                response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "action", action);
                cJSON_AddStringToObject(response, "statut", "400");
                cJSON_AddStringToObject(response, "message", "cannot start session");
            }
        }
        else if (strcmp(action, "question/answer") == 0) {
            if (handle_question_answer(request, current_session_id, sock)) {
                no_response = 1;
            } else {
                response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "action", action);
                cJSON_AddStringToObject(response, "statut", "400");
                cJSON_AddStringToObject(response, "message", "invalid answer");
            }
        }
        else if (strcmp(action, "joker/use") == 0) {
            // ✅ CORRECTION : Route joker/use ajoutée
            if (handle_joker_use(request, current_session_id, sock)) {
                no_response = 1;
            } else {
                response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "action", action);
                cJSON_AddStringToObject(response, "statut", "400");
                cJSON_AddStringToObject(response, "message", "cannot use joker");
            }
        }
        else if (strcmp(action, "chat/send") == 0) {
            // Route chat/send -> broadcast chat message
            if (handle_chat_send(request, current_session_id, sock)) {
                no_response = 1;
            } else {
                response = cJSON_CreateObject();
                cJSON_AddStringToObject(response, "action", action);
                cJSON_AddStringToObject(response, "statut", "400");
                cJSON_AddStringToObject(response, "message", "cannot send chat");
            }
        }
        else {
            response = cJSON_CreateObject();
            cJSON_AddStringToObject(response, "action", action);
            cJSON_AddStringToObject(response, "statut", "404");
            cJSON_AddStringToObject(response, "message", "action not implemented");
        }
        
        if (response && !no_response) {
            send_json_to_socket(sock, response);
            cJSON_Delete(response);
        }
        
        if (request) cJSON_Delete(request);
        
        if (no_response) continue;
    }
    
    close(sock);
    log_info("Client disconnected");
    return NULL;
}

// ============================================================================
// FONCTION PRINCIPALE
// ============================================================================
int main(void) {
    pthread_mutex_lock(&sessions_lock);
    for (int i = 0; i < MAX_SESSIONS; i++) {
        global_sessions[i].sessionId = -1;
    }
    pthread_mutex_unlock(&sessions_lock);
    
    global_users = load_users();
    load_questions("questions.json");
    
    pthread_t udp_thread;
    pthread_create(&udp_thread, NULL, udp_broadcast, NULL);
    pthread_detach(udp_thread);
    
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("TCP socket");
        return 1;
    }
    
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(TCP_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;
    
    if (bind(server_sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("TCP bind");
        close(server_sock);
        return 1;
    }
    
    if (listen(server_sock, 10) < 0) {
        perror("listen");
        close(server_sock);
        return 1;
    }
    
    log_info("Server started on TCP port %d", TCP_PORT);
    
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        int *client_sock = malloc(sizeof(int));
        *client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_len);
        
        if (*client_sock < 0) {
            free(client_sock);
            continue;
        }
        
        log_info("New client connected from %s", inet_ntoa(client_addr.sin_addr));
        
        pthread_t thread;
        pthread_create(&thread, NULL, client_handler, client_sock);
        pthread_detach(thread);
    }
    
    close(server_sock);
    cJSON_Delete(global_users);
    return 0;
}
