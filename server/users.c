#include "users.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <ctype.h>

#define USERS_FILE "users.json"
#define MIN_PSEUDO_LENGTH 3
#define MAX_PSEUDO_LENGTH 20
#define MIN_PASSWORD_LENGTH 4

static int is_valid_pseudo(const char *pseudo) {
    if (!pseudo) return 0;
    
    size_t len = strlen(pseudo);
    if (len < MIN_PSEUDO_LENGTH || len > MAX_PSEUDO_LENGTH) {
        return 0;
    }
    
    for (size_t i = 0; i < len; i++) {
        if (!isalnum((unsigned char)pseudo[i]) && pseudo[i] != '_' && pseudo[i] != '-') {
            return 0;
        }
    }
    
    return 1;
}

cJSON* load_users(void) {
    FILE *f = fopen(USERS_FILE, "r");
    if (!f) {
        cJSON *root = cJSON_CreateObject();
        if (!root) return NULL;
        
        cJSON_AddArrayToObject(root, "users");
        save_users(root);
        return root;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    if (fsize <= 0) {
        fclose(f);
        cJSON *root = cJSON_CreateObject();
        if (!root) return NULL;
        cJSON_AddArrayToObject(root, "users");
        return root;
    }
    
    char *buffer = malloc(fsize + 1);
    if (!buffer) {
        fclose(f);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, fsize, f);
    buffer[read_size] = '\0';
    fclose(f);
    
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root || !cJSON_GetObjectItem(root, "users")) {
        if (root) cJSON_Delete(root);
        root = cJSON_CreateObject();
        if (!root) return NULL;
        cJSON_AddArrayToObject(root, "users");
    }
    
    return root;
}

void save_users(cJSON *users) {
    if (!users) return;
    
    FILE *f = fopen(USERS_FILE, "w");
    if (!f) {
        fprintf(stderr, "Erreur: impossible d'ouvrir %s en écriture\n", USERS_FILE);
        return;
    }
    
    char *json_str = cJSON_Print(users);
    if (json_str) {
        fprintf(f, "%s", json_str);
        free(json_str);
    }
    
    fclose(f);
}

void hash_password(const char *password, char *output) {
    if (!password || !output) return;
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)password, strlen(password), hash);
    
    for(int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        sprintf(output + (i * 2), "%02x", hash[i]);
    }
    output[64] = '\0';
}

int add_user(cJSON *users, const char *username, const char *password) {
    if (!users || !username || !password) return 0;
    
    if (!is_valid_pseudo(username)) {
        fprintf(stderr, "Pseudo invalide: doit contenir %d-%d caractères alphanumériques\n",
                MIN_PSEUDO_LENGTH, MAX_PSEUDO_LENGTH);
        return 0;
    }
    
    if (strlen(password) < MIN_PASSWORD_LENGTH) {
        fprintf(stderr, "Mot de passe trop court: minimum %d caractères\n", MIN_PASSWORD_LENGTH);
        return 0;
    }
    
    cJSON *users_array = cJSON_GetObjectItem(users, "users");
    if (!users_array) {
        fprintf(stderr, "Erreur: structure JSON invalide\n");
        return 0;
    }
    
    cJSON *user = NULL;
    cJSON_ArrayForEach(user, users_array) {
        cJSON *pseudo = cJSON_GetObjectItem(user, "pseudo");
        if (pseudo && pseudo->valuestring && 
            strcmp(pseudo->valuestring, username) == 0) {
            return 0;
        }
    }
    
    char hashed[65];
    hash_password(password, hashed);
    
    cJSON *new_user = cJSON_CreateObject();
    if (!new_user) return 0;
    
    cJSON_AddStringToObject(new_user, "pseudo", username);
    cJSON_AddStringToObject(new_user, "password", hashed);
    cJSON_AddNumberToObject(new_user, "createdAt", (double)time(NULL));
    
    cJSON_AddItemToArray(users_array, new_user);
    
    save_users(users);
    return 1;
}

cJSON* check_login(cJSON *users, const char *username, const char *password) {
    if (!users || !username || !password) return NULL;
    
    cJSON *users_array = cJSON_GetObjectItem(users, "users");
    if (!users_array) return NULL;
    
    char hashed[65];
    hash_password(password, hashed);
    
    cJSON *user = NULL;
    cJSON_ArrayForEach(user, users_array) {
        cJSON *pseudo = cJSON_GetObjectItem(user, "pseudo");
        cJSON *pass = cJSON_GetObjectItem(user, "password");
        
        if (pseudo && pseudo->valuestring && pass && pass->valuestring) {
            if (strcmp(pseudo->valuestring, username) == 0 &&
                strcmp(pass->valuestring, hashed) == 0) {
                return user;
            }
        }
    }
    
    return NULL;
}
