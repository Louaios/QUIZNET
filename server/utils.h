#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Logging utilities */
void log_info(const char *format, ...);
void log_error(const char *format, ...);

/* String utilities */
char* trim(char *str);
int str_equals_ignore_case(const char *s1, const char *s2);

/* JSON helpers */
char* create_error_response(const char *action, const char *status, const char *message);

#endif
