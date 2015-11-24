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

typedef struct {
    Thash *hashtbl[NHASHROOT];
    int backed;
    int backch;
    char token_buff[MAXTOKEN + 4];
    char *token_text;
    int token_type;
    int back_token_type;
    char *back_token_text;
} TokenState;

static TokenState st_token_state_entity;
static TokenState *st_token_state = &st_token_state_entity;


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
char *token_intern(char *s){
    Thash *p, **root;

    root = st_token_state->hashtbl + (hash(s) % NHASHROOT);
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

static int get(){
    int c;

    if (st_token_state->backed) {
        st_token_state->backed = 0;
        return st_token_state->backch;
    }
    c = fgetc(ifp);
    if (c == '\n') lineno++;
    return c;
}

static void unget(int c){
    if (c == EOF) return;
    if (st_token_state->backed) die("too many unget");
    st_token_state->backed = 1;
    st_token_state->backch = c;
}


/*** buffered token scanner ***/

#define	issymch(c) (isdigit(c) || isalpha(c) || c == '_')

#define iswhite(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\v' || (c) == '\f')

char *token_get_current_text(){ return st_token_state->token_text; }

/*
 * Return next token from input.
 * return: token type
 * global: token_text Interned token text body
 */
int token_get_raw(){
    int c, tag;
    char *p;

    if (st_token_state->back_token_text) {
        st_token_state->token_type = st_token_state->back_token_type;
        st_token_state->token_text = st_token_state->back_token_text;
        st_token_state->back_token_type = 0;
        st_token_state->back_token_text = NULL;
        return st_token_state->token_type;
    }

    st_token_state->token_text = p = st_token_state->token_buff;
    c = get();
    if (iswhite(c)) {
        while (iswhite(c)) {
            if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get();
        }
        unget(c);
        *p = '\0';
        return st_token_state->token_type = SPACE;
    }
    if (c == '\n') {
        *p++ = c;
        *p = '\0';
        return st_token_state->token_type = NEWLINE;
    }
    if (c == '/') {
        if ((c = get()) == '*') {
            /* skip comments */
            *p++ = '/';
            *p++ = '*';
            for (;;) {
                if ((c = get()) == '*') {
                    if ((c = get()) == '/') break;
                    unget(c);
                }
                if (c == EOF) die("missing */");
                if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            }
            if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
            *p++ = '*';
            *p++ = '/';
            *p++ = '\0';
            return st_token_state->token_type = COMMENT;
        } else if (c == '/') {
            /* skip // comment */
            *p++ = '/';
            *p++ = '/';
            do {
                c = get();
                if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            } while (c != '\n' && c != EOF);
            *p++ = '\0';
            return st_token_state->token_type = COMMENT;
        }
        unget(c);
        c = '/';
    }

    if (c == EOF) return st_token_state->token_type = EOF;

    tag = c;
    if (c == '%') {
        c = get();
        if (c == '%' || c == '{' || c == '}' || issymch(c)) {
            *p++ = '%';
        } else {
            unget(c);
            c = '%';
        }
    }
    if (issymch(c)) {
        while (issymch(c)) {
            if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get();
        }
        unget(c);
        tag = isdigit(tag) ? NUMBER : NAME;
    } else if (c == '\'' || c == '"') {
        *p++ = c;
        while ((c = get()) != tag) {
            if (p >= st_token_state->token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            if (c == EOF || c == '\n') {
                error("missing '");
                break;
            }
            if (c == '\\') {
                c = get();
                if (c == EOF)
                    break;
                if (c == '\n')
                    continue;
                *p++ = c;
            }
#if MSKANJI
            if (iskanji(c)) {
                c = get();
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

    st_token_state->token_text = token_intern(st_token_state->token_buff);

    if (st_token_state->token_buff[0] == '%') {
        /* check keyword */
        if (strcmp(st_token_state->token_buff, "%%") == 0) {
            tag = MARK;
        } else if (strcmp(st_token_state->token_buff, "%{") == 0) {
            tag = BEGININC;
        } else if (strcmp(st_token_state->token_buff, "%}") == 0) {
            tag = ENDINC;
        } else if (strcmp(st_token_state->token_buff, "%token") == 0) {
            tag = TOKEN;
        } else if (strcmp(st_token_state->token_buff, "%term") == 0) {	/* old feature */
            tag = TOKEN;
        } else if (strcmp(st_token_state->token_buff, "%left") == 0) {
            tag = LEFT;
        } else if (strcmp(st_token_state->token_buff, "%right") == 0) {
            tag = RIGHT;
        } else if (strcmp(st_token_state->token_buff, "%nonassoc") == 0) {
            tag = NONASSOC;
        } else if (strcmp(st_token_state->token_buff, "%prec") == 0) {
            tag = PRECTOK;
        } else if (strcmp(st_token_state->token_buff, "%type") == 0) {
            tag = TYPE;
        } else if (strcmp(st_token_state->token_buff, "%union") == 0) {
            tag = UNION;
        } else if (strcmp(st_token_state->token_buff, "%start") == 0) {
            tag = START;
        } else if (strcmp(st_token_state->token_buff, "%expect") == 0) {
            tag = EXPECT;
        } else if (strcmp(st_token_state->token_buff, "%pure_parser") == 0) {
            tag = PURE_PARSER;
        }
    }
    return st_token_state->token_type = tag;

 toolong:
    die("Too long token");
    return 0;
}

int token_get(){
    int c = token_get_raw();
    while (c == SPACE || c == COMMENT || c == NEWLINE) {
        c = token_get_raw();
    }
    return c;
}

void token_unget(){
    if (st_token_state->back_token_text) die("too many token_unget");
    st_token_state->back_token_type = st_token_state->token_type;
    st_token_state->back_token_text = st_token_state->token_text;
}

/* Peek next token */
int token_peek(){
    int save_token_type = st_token_state->token_type;
    char *save_token_text = st_token_state->token_text;
    int tok = token_get();
    token_unget();
    st_token_state->token_text = save_token_text;
    st_token_state->token_type = save_token_type;
    return tok;
}
