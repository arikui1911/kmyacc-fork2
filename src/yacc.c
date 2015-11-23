/*
 * yacc.c - LALR parser generator
 *
 * Copyright (C) 1987,1989,1992,1993,2005  MORI Koichiro
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#if HAS_STDLIB
# include <stdlib.h>
#else
char *getenv();
#endif /* HAS_STDLIB */

#include "common.h"
#include "misc.h"
#include "token.h"

#include "grammar.h"
#include "lalr.h"
#include "genparser.h"
#include "yacc.h"

#include <assert.h>
#include <string.h>


global bool lflag;
global bool tflag;
global bool debug;
global bool aflag;
global bool nflag;
global bool iflag;

global FILE *ifp, *ofp, *vfp, *hfp;
global int lineno = 1;
global char *filename;
global char *outfilename;

global int expected_srconf;

global char *pspref;

static char *progname = "kmyacc";

#ifdef MSDOS
# define MAXPATHLEN 90
#else
# define MAXPATHLEN 200
#endif /* MSDOS */


/* Print usage of this program */
void usage(const char *program_name){
    fprintf(stderr, "KMyacc - parser generator ver 4.1.4\n");
    fprintf(stderr, "Copyright (C) 1987-1989,1992-1993,2005,2006  MORI Koichiro\n\n");
    fprintf(stderr, "Usage: %s [-dvltani] [-b y] [-p yy] [-m model] [-L lang] [grammar.y\n", program_name);
}


/* Open file fn with mode; exit if fail */
global FILE *efopen(char *fn, char *mode){
    FILE *fp;

    if (strcmp(fn, "-") == 0) {
        switch (*mode) {
        case 'r':
            return stdin;
        case 'w':
            return stdout;
        }
    }
    if ((fp = fopen(fn, mode)) == NULL) {
        fprintf(stderr, "%s: ", progname);
        perror(fn);
        exit(1);
    }
    return (fp);
}

/* Close file fp and exit if error */
global void efclose(FILE *fp){
    if (ferror(fp) || fclose(fp)) {
        fprintf(stderr, "%s: can't close\n", progname);
        exit(1);
    }
}

typedef struct {
    int fl_verbose;
    int fl_defines;
    char *filename_prefix;
    char *host_lang;
    char *parser_filename;
    char *parser_prefix;
} Options;

// return < 0: error
// return 0: no rest args
// return > 0: first arg's index for argv
static int parse_options(int argc, char *argv[], Options *o){
    int i, j;
    char *arg = NULL;
    char arg_requirer = '\0';
    int is_normal_arg;

    for (i = 1; i < argc; i++) {
        if (argv[i][0] == '\0') continue;   // empty
        if (strcmp("--", argv[i]) == 0) {
            i++;
            break;
        }

        is_normal_arg = 0;
        if (argv[i][0] != '-') {
            is_normal_arg = 1;
        } else if (strcmp("-", argv[i]) == 0) {
            is_normal_arg = 1;
        }

        if (arg) {
            int cur_arg_consumed = 0;
            if (arg[0] == '\0') {
                if (!is_normal_arg) break;
                arg = argv[i];
                cur_arg_consumed = 1;
            }
            switch (arg_requirer) {
            case 'b':
                o->filename_prefix = arg;
                break;
            case 'p':
                o->parser_prefix = arg;
                break;
            case 'm':
                o->parser_filename = arg;
                break;
            case 'L':
                o->host_lang = arg;
                break;
            default:
                assert(0);
            }
            arg = NULL;
            if (cur_arg_consumed) continue;
        }

        if (is_normal_arg) break;

        for (j = 1; argv[i][j] != '\0'; j++) {
            switch (argv[i][j]) {
            case 'd':
                o->fl_defines = 1;
                break;
            case 'x':
                debug = 1;
                o->fl_verbose = 1;
                break;
            case 'v':
                o->fl_verbose = 1;
                break;
            case 'l':
                lflag = 1;
                break;
            case 't':
                tflag = 1;
                break;
            case 'i':
                iflag = 1;
                nflag = 1;
                break;
            case 'n':
                nflag = 1;
                break;
            case 'a':
                aflag = 1;
                break;
            case 'b':
            case 'p':
            case 'm':
            case 'L':
                arg = &argv[i][j+1];
                arg_requirer = argv[i][j];
                break;
            default:
                fprintf(stderr, "%s: error: invalid argument `-%c'\n", argv[0], argv[i][j]);
                return -1;
            }
        }
    }
    if (arg) {
        fprintf(stderr, "%s: error: option `-%c' missing argument\n", argv[0], arg_requirer);
        return -2;
    }

    if (i >= argc) return 0;
    return i;
}

#ifdef MSDOS
# define OUT_SUFFIX ".out"
#else
# define OUT_SUFFIX ".output"
# define remove unlink
#endif /* MSDOS */

/* Entry point */
int main(int argc, char *argv[]){
    char fn[MAXPATHLEN];
    char *parser_filename;

    Options options;
    int args_idx;

    char *host_lang = NULL;
    bool fl_verbose = NO;
    bool fl_defines = NO;
    char *filename_prefix = NULL;

    parser_filename = getenv("KMYACCPAR");

#ifndef MSDOS
    progname = *argv;
#endif /* !MSDOS */

    args_idx = parse_options(argc, argv, &options);
    if (args_idx < 1 || (argc - args_idx) > 0) {
        usage(argv[0]);
        return -1;
    }

    filename = argv[args_idx];

    if (host_lang) {
        parser_set_language(host_lang);
    } else {
        parser_set_language_by_yaccext(extension(filename));
    }

    ifp = efopen(filename, "r");
    outfilename = parser_outfilename(filename_prefix, filename); 
    ofp = efopen(outfilename, "w");
    if (fl_defines) {
        char *header = parser_header_filename(filename_prefix, filename);
        if (header != NULL) hfp = efopen(header, "w");
    }
    if (parser_filename == NULL) parser_filename = parser_modelfilename(PARSERBASE);

    parser_create(efopen(parser_filename, "r"), parser_filename, tflag);
    do_declaration();
    do_grammar();

    if (worst_error == 0) {
        if (fl_verbose) vfp = efopen(strcat(strcpy(fn, filename_prefix ? filename_prefix : "y"), OUT_SUFFIX), "w");
        comp_lalr();        /* compute LALR(1) states & actions */
        parser_generate();
        if (vfp) efclose(vfp);
    }

    parser_close();
    efclose(ofp);
    exit(worst_error);
}
