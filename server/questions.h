#ifndef QUESTIONS_H
#define QUESTIONS_H

#define MAX_QUESTIONS 100
#define MAX_QUESTION_TEXT 256
#define MAX_ANSWERS 4
#define MAX_ANSWER_TEXT 128

typedef struct {
    int id;
    char question[MAX_QUESTION_TEXT];
    char answers[MAX_ANSWERS][MAX_ANSWER_TEXT];
    int correct_index;
    int type; // 0=qcm, 1=boolean, 2=text
    int difficulty;
    char theme[32];
} Question;

/* Table des questions */
extern Question questions[MAX_QUESTIONS];
extern int question_count;

/* Charger les questions depuis questions.json */
void load_questions(const char *filename);

/* Sélection aléatoire de nb questions selon difficulty et theme */
int select_questions(Question *dest, int nb, int difficulty, const char *theme);

/* Conversion Question -> JSON */
char* question_to_json(const Question *q);

#endif
