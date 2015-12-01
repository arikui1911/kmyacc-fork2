#ifndef _genparser_h
#define _genparser_h
#include <stdio.h>

/* Language ID */
#define LANG_C		0
#define LANG_CPP	1
#define LANG_JAVA	2
#define LANG_PERL	3
#define LANG_RUBY	4
#define LANG_JS		5
#define LANG_ML		6

char *get_lang_name(void);
int get_lang_id(void);
void parser_set_language(char *langname);
void parser_set_language_by_yaccext(char *ext);
char *parser_outfilename(char *pref, char *yacc_filename);
char *parser_modelfilename(char *parser_base);
char *parser_header_filename(char *pref, char *yacc_filename);
void parser_create(FILE *fp, char *fn, bool tflag);
void parser_begin_copying(void);
void parser_copy_token(char *str);
void parser_end_copying(void);
char *parser_dollar(int dollartype, int nth, int len, char *typename);
void parser_generate(void);
void parser_close(void);

#endif
