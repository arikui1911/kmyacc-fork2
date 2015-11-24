/*
 * token.c - lexical analyzer for yacc
 *
 * Copyright (C) 1993, 2005    MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if HAS_STDLIB
# include <stdlib.h>
#endif /* HAS_STDLIB */

#include "./common.h"
#include "./yacc.h"
#include "./token.h"
#include "./misc.h"


typedef struct thash Thash;

#define NHASHROOT 512

#define MAXTOKEN 50000

struct TokenState {
    Thash *hashtbl[NHASHROOT];
    int backed;
    int backch;
    char token_buff[MAXTOKEN + 4];
    char *token_text;
    int token_type;
    int back_token_type;
    char *back_token_text;
};

static TokenState st_token_state_entity;
static TokenState *st_token_state = &st_token_state_entity;

TokenState *token_get_current_state(void){
    return st_token_state;
}

/*** Intern table ***/

/* Intern token strings. */
struct thash {
    struct thash *next;
    char body[1];
};

/* Return string p's hash value */
static unsigned hash(char *p){
    unsigned u;

    u = 0;
    while (*p) {
        u = u * 257 + *p++;
    }
    return (u);
}

/* Intern token s */
char *token_intern(TokenState *t, char *s){
    Thash *p, **root;

    root = t->hashtbl + (hash(s) % NHASHROOT);
    for (p = *root; p != NULL; p = p->next) {
        if (strcmp(p->body, s) == 0) return p->body;
    }
    p = alloc(sizeof(Thash) + strlen(s));
    p->next = *root;
    *root = p;
    strcpy(p->body, s);
    return p->body;
}


/*** buffered char reader ***/

static int get(TokenState *t){
    int c;

    if (t->backed) {
        t->backed = 0;
        return t->backch;
    }
    c = fgetc(ifp);
    if (c == '\n') lineno++;
    return c;
}

static void unget(TokenState *t, int c){
    if (c == EOF) return;
    if (t->backed) die("too many unget");
    t->backed = 1;
    t->backch = c;
}


/*** buffered token scanner ***/

#define	issymch(c) (isdigit(c) || isalpha(c) || c == '_')

#define iswhite(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\v' || (c) == '\f')

char *token_get_current_text_func(TokenState *t){ return t->token_text; }

/*
 * Return next token from input.
 * return: token type
 * global: token_text Interned token text body
 */
int token_get_raw(TokenState *t){
    int c, tag;
    char *p;

    if (t->back_token_text) {
        t->token_type = t->back_token_type;
        t->token_text = t->back_token_text;
        t->back_token_type = 0;
        t->back_token_text = NULL;
        return t->token_type;
    }

    t->token_text = p = t->token_buff;
    c = get(t);
    if (iswhite(c)) {
        while (iswhite(c)) {
            if (p >= t->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get(t);
        }
        unget(t, c);
        *p = '\0';
        return t->token_type = SPACE;
    }
    if (c == '\n') {
        *p++ = c;
        *p = '\0';
        return t->token_type = NEWLINE;
    }
    if (c == '/') {
        if ((c = get(t)) == '*') {
            /* skip comments */
            *p++ = '/';
            *p++ = '*';
            for (;;) {
                if ((c = get(t)) == '*') {
                    if ((c = get(t)) == '/') break;
                    unget(t, c);
                }
                if (c == EOF) die("missing */");
                if (p >= t->token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            }
            if (p >= t->token_buff + MAXTOKEN) goto toolong;
            *p++ = '*';
            *p++ = '/';
            *p++ = '\0';
            return t->token_type = COMMENT;
        } else if (c == '/') {
            /* skip // comment */
            *p++ = '/';
            *p++ = '/';
            do {
                c = get(t);
                if (p >= t->token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            } while (c != '\n' && c != EOF);
            *p++ = '\0';
            return t->token_type = COMMENT;
        }
        unget(t, c);
        c = '/';
    }

    if (c == EOF) return t->token_type = EOF;

    tag = c;
    if (c == '%') {
        c = get(t);
        if (c == '%' || c == '{' || c == '}' || issymch(c)) {
            *p++ = '%';
        } else {
            unget(t, c);
            c = '%';
        }
    }
    if (issymch(c)) {
        while (issymch(c)) {
            if (p >= t->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get(t);
        }
        unget(t, c);
        tag = isdigit(tag) ? NUMBER : NAME;
    } else if (c == '\'' || c == '"') {
        *p++ = c;
        while ((c = get(t)) != tag) {
            if (p >= t->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            if (c == EOF || c == '\n') {
                error("missing '");
                break;
            }
            if (c == '\\') {
                c = get(t);
                if (c == EOF)
                    break;
                if (c == '\n')
                    continue;
                *p++ = c;
            }
#if MSKANJI
            if (iskanji(c)) {
                c = get(t);
                if (!iskanji2(c)) {
                    error("bad kanji code\n");
                }
                *p++ = c;
            }
#endif
        }
        *p++ = c;
    } else {
        *p++ = c;
    }
    *p++ = '\0';

    t->token_text = token_intern(t, t->token_buff);

    if (t->token_buff[0] == '%') {
        /* check keyword */
        if (strcmp(t->token_buff, "%%") == 0) {
            tag = MARK;
        } else if (strcmp(t->token_buff, "%{") == 0) {
            tag = BEGININC;
        } else if (strcmp(t->token_buff, "%}") == 0) {
            tag = ENDINC;
        } else if (strcmp(t->token_buff, "%token") == 0) {
            tag = TOKEN;
        } else if (strcmp(t->token_buff, "%term") == 0) {	/* old feature */
            tag = TOKEN;
        } else if (strcmp(t->token_buff, "%left") == 0) {
            tag = LEFT;
        } else if (strcmp(t->token_buff, "%right") == 0) {
            tag = RIGHT;
        } else if (strcmp(t->token_buff, "%nonassoc") == 0) {
            tag = NONASSOC;
        } else if (strcmp(t->token_buff, "%prec") == 0) {
            tag = PRECTOK;
        } else if (strcmp(t->token_buff, "%type") == 0) {
            tag = TYPE;
        } else if (strcmp(t->token_buff, "%union") == 0) {
            tag = UNION;
        } else if (strcmp(t->token_buff, "%start") == 0) {
            tag = START;
        } else if (strcmp(t->token_buff, "%expect") == 0) {
            tag = EXPECT;
        } else if (strcmp(t->token_buff, "%pure_parser") == 0) {
            tag = PURE_PARSER;
        }
    }
    return t->token_type = tag;

 toolong:
    die("Too long token");
    return 0;
}

int token_get(TokenState *t){
    int c = token_get_raw(t);
    while (c == SPACE || c == COMMENT || c == NEWLINE) {
        c = token_get_raw(t);
    }
    return c;
}

void token_unget(TokenState *t){
    if (t->back_token_text) die("too many token_unget");
    t->back_token_type = t->token_type;
    t->back_token_text = t->token_text;
}

/* Peek next token */
int token_peek(TokenState *t){
    int save_token_type = t->token_type;
    char *save_token_text = t->token_text;
    int tok = token_get(t);
    token_unget(t);
    t->token_text = save_token_text;
    t->token_type = save_token_type;
    return tok;
}
