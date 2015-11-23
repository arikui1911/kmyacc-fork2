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

char *intern_token(char *s);
int   raw_gettoken(void);
int   gettoken(void);
void  ungettok(void);
int   peektoken(void);

char *token_get_current_text();

#endif
