#include <string.h>
#include "buffer.h"

void buffer_initialize(struct buffer *b, lua_State *L) {
    b->L = L;
    b->head = (struct block*)&b->stack;
    b->head->p = 0;
    b->head->len = INITIAL_SIZE;
    b->head->next = NULL;
    b->curr = b->head;
}

static struct block *_buffer_new_block(struct buffer *b) {
    void *ud;
    lua_Alloc alloc = lua_getallocf(b->L, &ud);
    struct block *res = (struct block*)alloc(ud, NULL, 0, b->curr->len * 2 + sizeof(struct block));
    res->p = 0;
    res->len = b->curr->len * 2;
    res->next = NULL;
    return res;
}

void buffer_append(struct buffer *b, const char *data, size_t len) {
    size_t space = b->curr->len - b->curr->p;
    while (space < len) {
        if (space > 0) {
            memcpy(b->curr->data + b->curr->p, data, space);
            data += space;
            b->curr->p += space;
            len -= space;
        }
        b->curr = b->curr->next = _buffer_new_block(b);
        space = b->curr->len;
    }
    memcpy(b->curr->data + b->curr->p, data, len);
    b->curr->p += len;
}

void buffer_free(struct buffer *b) {
    void *ud;
    lua_Alloc alloc = lua_getallocf(b->L, &ud);
    struct block *p = b->head;
    while (p) {
        struct block *t = p->next;
        if (p != (struct block*)&b->stack)
            alloc(ud, p, p->len + sizeof(struct block), 0);
        p = t;
    }
}

void buffer_push_string(struct buffer *b) {
    size_t size = buffer_size(b);
    if (size <= INITIAL_SIZE) {
        lua_pushlstring(b->L, b->head->data, size);
    } else {
        void *ud;
        lua_Alloc alloc = lua_getallocf(b->L, &ud);

        char *str = (char *)alloc(ud, NULL, 0, size);
        char *s = str;
        struct block *p = b->head;
        while (p) {
            memcpy(s, p->data, p->p);
            s += p->p;
            p = p->next;
        }

        lua_pushlstring(b->L, str, size);
        alloc(ud, str, size, 0);
    }
}
