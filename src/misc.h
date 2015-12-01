#ifndef _misc_h
#define _misc_h

typedef struct list {
    struct list *next;
    void *elem;
} List;

typedef struct flexstr Flexstr;

struct flexstr {
    int alloc_size;
    int length;
    char *body;
};

char *extension(char *path);
void error1(char *fmt, char *x);
void error(char *fmt);
void die(char *msg);
void die1(char *msg, char *str);
List *nreverse(List *p);
List *sortlist(List *p, int (*compar)());
Flexstr *new_flexstr(int defaultsize);
void copy_flexstr(Flexstr *fap, char *str);
void append_flexstr(Flexstr *fap, char *str);
void resize_flexstr(Flexstr *fap, int requiredsize);
void *alloc(unsigned s);
void *talloc(unsigned s);
void release(void);
long bytes_allocated(void);
void show_mem_usage(void);

#endif
