#ifndef _token_h
#define _token_h

/* return values of function gettoken */
#define	NAME		'a'
#define	NUMBER		'0'
#define SPACE		' '
#define NEWLINE		'\n'
#define	MARK		0x100
#define	BEGININC	0x101
#define	ENDINC		0x102
#define	TOKEN		0x103
#define	LEFT		0x104
#define	RIGHT		0x105
#define	NONASSOC	0x106
#define	PRECTOK		0x107
#define	TYPE		0x108
#define	UNION		0x109
#define	START		0x10a
#define COMMENT		0x10b

#define EXPECT		0x10c
#define PURE_PARSER	0x10d

typedef struct TokenState TokenState;

TokenState *token_get_current_state(void);

char *token_intern(TokenState *, char *);
int   token_get_raw(TokenState *);
int   token_get(TokenState *);
void  token_unget(TokenState *);
int   token_peek(TokenState *);
char *token_get_current_text_func(TokenState *);

/* temporary alternatives */
#define intern_token(s)     (token_intern(token_get_current_state(), (s)))
#define raw_gettoken()      (token_get_raw(token_get_current_state()))
#define gettoken()          (token_get(token_get_current_state()))
#define ungettok()          (token_unget(token_get_current_state()))
#define peektoken()         (token_peek(token_get_current_state()))
#define token_get_current_text()    (token_get_current_text_func(token_get_current_state()))

#endif
