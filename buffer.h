#ifndef _BUFFER_H_
#define _BUFFER_H_

#include <lua.h>

#define INITIAL_SIZE 1024

struct block {
    int p;
    int len;
    struct block *next;
    char data[];
};

struct buffer {
    lua_State *L;
    struct block *head;
    struct block *curr;
    struct {
        int p;
        int len;
        struct block *next;
        char data[INITIAL_SIZE];
    } stack;
};

void buffer_initialize(struct buffer *b, lua_State *L);
void buffer_append(struct buffer *b, const char *data, size_t len);
void buffer_free(struct buffer *b);
void buffer_push_string(struct buffer *b);

inline static void buffer_append_char(struct buffer *b, char c) {
    buffer_append(b, &c, 1);
}

#define buffer_append_str(b, str) buffer_append((b), (str), strlen(str))
#define buffer_append_lstr buffer_append

#define buffer_size(b) ((b)->curr->len - INITIAL_SIZE + (b)->curr->p)

#endif //_BUFFER_H_
