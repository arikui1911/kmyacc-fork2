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


/* Intern token strings. */
typedef struct thash Thash;
struct thash {
    struct thash *next;
    char body[1];
};
#define NHASHROOT 512
// Thash *hashtbl[NHASHROOT];

typedef struct {
    Thash *hashtbl[NHASHROOT];
    int backed;
    int backch;
    char *token_text;
    int token_type;
    int back_token_type;
    char *back_token_text;
} TokenState;

static TokenState st_token_state;



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

    root = hashtbl + (hash(s) % NHASHROOT);
    for (p = *root; p != NULL; p = p->next) {
        if (strcmp(p->body, s) == 0) return p->body;
    }
    p = alloc(sizeof(Thash) + strlen(s));
    p->next = *root;
    *root = p;
    strcpy(p->body, s);
    return p->body;
}


static bool backed;
static int backch;

static int get(){
    int c;

    if (backed) {
        backed = NO;
        return backch;
    }
    if ((c = getc(ifp)) == '\n') lineno++;
    return c;
}


static void unget(int c){
    if (c == EOF) return;
    if (backed) die("too many unget");
    backed = YES;
    backch = c;
}


/*#define issymch(c) (isdigit(c) || isalpha(c) || c == '_' || c == '.') */
#define	issymch(c) (isdigit(c) || isalpha(c) || c == '_')

#define iswhite(c) ((c) == ' ' || (c) == '\t' || (c) == '\r' || (c) == '\v' || (c) == '\f')

/* token_get changing text and type */
static char *token_text;
static int token_type;

char *token_get_current_text(){ return token_text; }

static int back_token_type;
static char *back_token_text;

#define MAXTOKEN 50000

/*
 * Return next token from input.
 * return: token type
 * global: token_text Interned token text body
 */
int token_get_raw(){
    int c, tag;
    char *p;
    static char token_buff[MAXTOKEN + 4];

    if (back_token_text) {
        token_type = back_token_type;
        token_text = back_token_text;
        back_token_type = 0;
        back_token_text = NULL;
        return token_type;
    }

    token_text = p = token_buff;
    c = get();
    if (iswhite(c)) {
        while (iswhite(c)) {
            if (p >= token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get();
        }
        unget(c);
        *p = '\0';
        return token_type = SPACE;
    }
    if (c == '\n') {
        *p++ = c;
        *p = '\0';
        return token_type = NEWLINE;
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
                if (p >= token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            }
            if (p >= token_buff + MAXTOKEN) goto toolong;
            *p++ = '*';
            *p++ = '/';
            *p++ = '\0';
            return token_type = COMMENT;
        } else if (c == '/') {
            /* skip // comment */
            *p++ = '/';
            *p++ = '/';
            do {
                c = get();
                if (p >= token_buff + MAXTOKEN) goto toolong;
                *p++ = c;
            } while (c != '\n' && c != EOF);
            *p++ = '\0';
            return token_type = COMMENT;
        }
        unget(c);
        c = '/';
    }

    if (c == EOF) return token_type = EOF;

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
            if (p >= token_buff + MAXTOKEN) goto toolong;
            *p++ = c;
            c = get();
        }
        unget(c);
        tag = isdigit(tag) ? NUMBER : NAME;
    } else if (c == '\'' || c == '"') {
        *p++ = c;
        while ((c = get()) != tag) {
            if (p >= token_buff + MAXTOKEN) goto toolong;
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

    token_text = token_intern(token_buff);

    if (token_buff[0] == '%') {
        /* check keyword */
        if (strcmp(token_buff, "%%") == 0) {
            tag = MARK;
        } else if (strcmp(token_buff, "%{") == 0) {
            tag = BEGININC;
        } else if (strcmp(token_buff, "%}") == 0) {
            tag = ENDINC;
        } else if (strcmp(token_buff, "%token") == 0) {
            tag = TOKEN;
        } else if (strcmp(token_buff, "%term") == 0) {	/* old feature */
            tag = TOKEN;
        } else if (strcmp(token_buff, "%left") == 0) {
            tag = LEFT;
        } else if (strcmp(token_buff, "%right") == 0) {
            tag = RIGHT;
        } else if (strcmp(token_buff, "%nonassoc") == 0) {
            tag = NONASSOC;
        } else if (strcmp(token_buff, "%prec") == 0) {
            tag = PRECTOK;
        } else if (strcmp(token_buff, "%type") == 0) {
            tag = TYPE;
        } else if (strcmp(token_buff, "%union") == 0) {
            tag = UNION;
        } else if (strcmp(token_buff, "%start") == 0) {
            tag = START;
        } else if (strcmp(token_buff, "%expect") == 0) {
            tag = EXPECT;
        } else if (strcmp(token_buff, "%pure_parser") == 0) {
            tag = PURE_PARSER;
        }
    }
    return token_type = tag;

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
    if (back_token_text) die("too many token_unget");
    back_token_type = token_type;
    back_token_text = token_text;
}

/* Peek next token */
int token_peek(){
    int save_token_type = token_type;
    char *save_token_text = token_text;
    int tok = token_get();
    token_unget();
    token_text = save_token_text;
    token_type = save_token_type;
    return tok;
}
