#include "questions.h"
#include "utils.h"
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <cJSON.h>

Question questions[MAX_QUESTIONS];
int question_count = 0;

/* trimming le */
static void trim_inplace(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}
void load_questions(const char *filename) {
    FILE *f = fopen(filename, "r");
    if (!f) {
        fprintf(stderr, "Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long fsize = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char *buffer = malloc(fsize + 1);
    fread(buffer, 1, fsize, f);
    buffer[fsize] = '\0';
    fclose(f);
    
    cJSON *root = cJSON_Parse(buffer);
    free(buffer);
    
    if (!root) {
        fprintf(stderr, "Erreur: JSON invalide\n");
        return;
    }
    
    cJSON *questions_array = cJSON_GetObjectItem(root, "questions");
    if (!questions_array) {
        cJSON_Delete(root);
        return;
    }
    
    question_count = 0;
    cJSON *q = NULL;
    cJSON_ArrayForEach(q, questions_array) {
        if (question_count >= MAX_QUESTIONS) break;
        
        cJSON *id = cJSON_GetObjectItem(q, "id");
        cJSON *question = cJSON_GetObjectItem(q, "question");
        cJSON *answers = cJSON_GetObjectItem(q, "answers");
        cJSON *correct = cJSON_GetObjectItem(q, "correct");
        cJSON *difficulty = cJSON_GetObjectItem(q, "difficulty");
        cJSON *themes = cJSON_GetObjectItem(q, "themes");
        
        if (id && question && answers && correct && difficulty) {
            Question *dest = &questions[question_count];
            
            dest->id = id->valueint;
            if (cJSON_IsString(question) && question->valuestring) {
                strncpy(dest->question, question->valuestring, MAX_QUESTION_TEXT - 1);
                dest->question[MAX_QUESTION_TEXT - 1] = '\0';
                trim_inplace(dest->question);
            } else {
                dest->question[0] = '\0';
            }
            dest->correct_index = correct->valueint;
            
            if (strcmp(difficulty->valuestring, "facile") == 0) dest->difficulty = 0;
            else if (strcmp(difficulty->valuestring, "moyen") == 0) dest->difficulty = 1;
            else dest->difficulty = 2;
            
            if (themes && cJSON_IsArray(themes)) {
                cJSON *theme = cJSON_GetArrayItem(themes, 0);
                    if (theme && theme->valuestring) {
                    strncpy(dest->theme, theme->valuestring, 31);
                    dest->theme[31] = '\0';
                }
            }
            
            int ans_count = 0;
            cJSON *answer = NULL;
            cJSON_ArrayForEach(answer, answers) {
                if (ans_count >= MAX_ANSWERS) break;
                    if (cJSON_IsString(answer) && answer->valuestring) {
                    strncpy(dest->answers[ans_count], answer->valuestring, MAX_ANSWER_TEXT - 1);
                    dest->answers[ans_count][MAX_ANSWER_TEXT - 1] = '\0';
                    trim_inplace(dest->answers[ans_count]);
                    ans_count++;
                }
            }
            
            while (ans_count < MAX_ANSWERS) {
                dest->answers[ans_count][0] = '\0';
                ans_count++;
            }
            
            question_count++;
        }
    }
    
    cJSON_Delete(root);
}

int select_questions(Question *dest, int nb, int difficulty, const char *theme) {
    if (nb > question_count) nb = question_count;
    
    int available[MAX_QUESTIONS];
    int available_count = 0;
    
    for (int i = 0; i < question_count; i++) {
        if (difficulty < 0 || questions[i].difficulty == difficulty) {
            if (theme == NULL || strcmp(theme, "all") == 0 || 
                strstr(questions[i].theme, theme) != NULL) {
                available[available_count++] = i;
            }
        }
    }
    
    if (available_count == 0) {
        for (int i = 0; i < question_count; i++) {
            available[available_count++] = i;
        }
    }
    
    srand(time(NULL));
    
    int selected = 0;
    for (int i = 0; i < nb && available_count > 0; i++) {
        int idx = rand() % available_count;
        int question_idx = available[idx];
        
        // copy struct
        dest[selected] = questions[question_idx];
        // diagnostic: if text-type question but question string empty, log it
        if (strlen(dest[selected].question) == 0) {
            printf("[WARN] select_questions: selected question id=%d has empty text\n", dest[selected].id);
        }
        selected++;
        
        available[idx] = available[available_count - 1];
        available_count--;
    }
    
    return selected;
}

char* question_to_json(const Question *q) {
    cJSON *json = cJSON_CreateObject();
    
    cJSON_AddNumberToObject(json, "id", q->id);
    cJSON_AddStringToObject(json, "question", q->question);
    cJSON_AddNumberToObject(json, "correct", q->correct_index);
    
    const char *diff[] = {"facile", "moyen", "difficile"};
    cJSON_AddStringToObject(json, "difficulty", diff[q->difficulty]);
    
    cJSON *answers = cJSON_CreateArray();
    for (int i = 0; i < MAX_ANSWERS && strlen(q->answers[i]) > 0; i++) {
        cJSON_AddItemToArray(answers, cJSON_CreateString(q->answers[i]));
    }
    cJSON_AddItemToObject(json, "answers", answers);
    
    char *result = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);
    
    return result;
}
