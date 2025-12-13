#include "utils.h"
#include <stdarg.h>
#include <ctype.h>

void log_info(const char *format, ...) {
    time_t now = time(NULL);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';
    
    printf("[INFO %s] ", timestamp);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void log_error(const char *format, ...) {
    time_t now = time(NULL);
    char timestamp[26];
    ctime_r(&now, timestamp);
    timestamp[24] = '\0';
    
    fprintf(stderr, "[ERROR %s] ", timestamp);
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fprintf(stderr, "\n");
}

char* trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

int str_equals_ignore_case(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        if (tolower((unsigned char)*s1) != tolower((unsigned char)*s2))
            return 0;
        s1++;
        s2++;
    }
    return *s1 == *s2;
}

char* create_error_response(const char *action, const char *status, const char *message) {
    char *response = malloc(512);
    if (action) {
        snprintf(response, 512, "{\"action\":\"%s\",\"statut\":\"%s\",\"message\":\"%s\"}", 
                 action, status, message);
    } else {
        snprintf(response, 512, "{\"statut\":\"%s\",\"message\":\"%s\"}", status, message);
    }
    return response;
}
