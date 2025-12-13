#ifndef JOKERS_H
#define JOKERS_H

#include "sessions.h"

#define JOKER_5050 0
#define JOKER_SKIP 1

char* apply_joker(Session *s, int playerIndex, const char *jokerType);
char* apply_5050(Session *s, int playerIndex);
char* apply_skip(Session *s, int playerIndex);

#endif
