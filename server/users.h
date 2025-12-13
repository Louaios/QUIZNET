#ifndef USERS_H
#define USERS_H

#include <time.h>

#ifdef __has_include
  #if __has_include(<cjson/cJSON.h>)
    #include <cjson/cJSON.h>
  #elif __has_include(<cJSON.h>)
    #include <cJSON.h>
  #else
    #error "cJSON header not found"
  #endif
#else
  #include <cjson/cJSON.h>
#endif

cJSON* load_users(void);
void save_users(cJSON *users);
void hash_password(const char *password, char *output);
int add_user(cJSON *users, const char *username, const char *password);
cJSON* check_login(cJSON *users, const char *username, const char *password);

#endif
