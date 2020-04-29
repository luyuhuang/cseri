#include <lauxlib.h>
#include <stdint.h>
#include <string.h>
#include "common.h"
#include "buffer.h"

#define TYPE_NIL 0
#define TYPE_BOOLEAN 1
// hibits 0 false 1 true

#define TYPE_NUMBER 2
// hibits 0 : 0 , 1: byte, 2:word, 4: dword, 6: qword, 8 : double
#define TYPE_NUMBER_ZERO 0
#define TYPE_NUMBER_BYTE 1
#define TYPE_NUMBER_WORD 2
#define TYPE_NUMBER_DWORD 4
#define TYPE_NUMBER_QWORD 6
#define TYPE_NUMBER_REAL 8

#define TYPE_USERDATA 3
#define TYPE_SHORT_STRING 4
// hibits 0~31 : len
#define TYPE_LONG_STRING 5
#define TYPE_TABLE 6

#define MAX_COOKIE 32
#define COMBINE_TYPE(t,v) ((t) | (v) << 3)

#define buffer_append(bf, data, len) buffer_append(bf, (char*)data, len)

/* dummy union to get native endianness */
static const union {
  int dummy;
  char little;  /* true iff machine is little endian */
} nativeendian = {1};

inline static void
_convert(char *p, size_t size) {
    if (!nativeendian.little) return;
    for (size_t i = 0; i < size / 2; ++i) {
        char t = p[i];
        p[i] = p[size - i - 1];
        p[size - i - 1] = t;
    }
}

#define CONVERT(n) _convert((char*)&(n), sizeof(n))

static inline void
append_nil(struct buffer *bf) {
    int n = TYPE_NIL;
    buffer_append(bf, &n, 1);
}

static inline void
append_boolean(struct buffer *bf, int boolean) {
    int n = COMBINE_TYPE(TYPE_BOOLEAN , boolean ? 1 : 0);
    buffer_append(bf, &n, 1);
}

static inline void
append_integer(struct buffer *bf, lua_Integer v) {
    int type = TYPE_NUMBER;
    if (v == 0) {
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_ZERO);
        buffer_append(bf, &n, 1);
    } else if (v != (int32_t)v) {
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_QWORD);
        int64_t v64 = v;
        CONVERT(v64);
        buffer_append(bf, &n, 1);
        buffer_append(bf, &v64, sizeof(v64));
    } else if (v < 0) {
        int32_t v32 = (int32_t)v;
        CONVERT(v32);
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_DWORD);
        buffer_append(bf, &n, 1);
        buffer_append(bf, &v32, sizeof(v32));
    } else if (v<0x100) {
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_BYTE);
        buffer_append(bf, &n, 1);
        uint8_t byte = (uint8_t)v;
        buffer_append(bf, &byte, sizeof(byte));
    } else if (v<0x10000) {
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_WORD);
        buffer_append(bf, &n, 1);
        uint16_t word = (uint16_t)v;
        CONVERT(word);
        buffer_append(bf, &word, sizeof(word));
    } else {
        uint8_t n = COMBINE_TYPE(type , TYPE_NUMBER_DWORD);
        buffer_append(bf, &n, 1);
        uint32_t v32 = (uint32_t)v;
        CONVERT(v32);
        buffer_append(bf, &v32, sizeof(v32));
    }
}

static inline void
append_real(struct buffer *bf, double v) {
    uint8_t n = COMBINE_TYPE(TYPE_NUMBER , TYPE_NUMBER_REAL);
    buffer_append(bf, &n, 1);
    buffer_append(bf, &v, sizeof(v));
}

static inline void
append_string(struct buffer *bf, const char *str, int len) {
    if (len < MAX_COOKIE) {
        uint8_t n = COMBINE_TYPE(TYPE_SHORT_STRING, len);
        buffer_append(bf, &n, 1);
        if (len > 0) {
            buffer_append(bf, str, len);
        }
    } else {
        uint8_t n;
        if (len < 0x10000) {
            n = COMBINE_TYPE(TYPE_LONG_STRING, 2);
            buffer_append(bf, &n, 1);
            uint16_t x = (uint16_t)len;
            CONVERT(x);
            buffer_append(bf, &x, 2);
        } else {
            n = COMBINE_TYPE(TYPE_LONG_STRING, 4);
            buffer_append(bf, &n, 1);
            uint32_t x = (uint32_t) len;
            CONVERT(x);
            buffer_append(bf, &x, 4);
        }
        buffer_append(bf, str, len);
    }
}

static void pack_one(lua_State *L, struct buffer *b, int index, int depth);

static int
append_table_array(lua_State *L, struct buffer *bf, int index, int depth) {
    int array_size = lua_rawlen(L,index);
    if (array_size >= MAX_COOKIE-1) {
        int n = COMBINE_TYPE(TYPE_TABLE, MAX_COOKIE-1);
        buffer_append(bf, &n, 1);
        append_integer(bf, array_size);
    } else {
        int n = COMBINE_TYPE(TYPE_TABLE, array_size);
        buffer_append(bf, &n, 1);
    }

    int i;
    for (i=1;i<=array_size;i++) {
        lua_rawgeti(L,index,i);
        pack_one(L, bf, -1, depth);
        lua_pop(L,1);
    }

    return array_size;
}

static void
append_table_hash(lua_State *L, struct buffer *bf, int index, int depth, int array_size) {
    lua_pushnil(L);
    while (lua_next(L, index) != 0) {
        if (lua_type(L,-2) == LUA_TNUMBER) {
            if (lua_isinteger(L, -2)) {
                lua_Integer x = lua_tointeger(L,-2);
                if (x>0 && x<=array_size) {
                    lua_pop(L,1);
                    continue;
                }
            }
        }
        pack_one(L,bf,-2,depth);
        pack_one(L,bf,-1,depth);
        lua_pop(L, 1);
    }
    append_nil(bf);
}

static void
pack_table(lua_State *L, struct buffer *bf, int index, int depth) {
    luaL_checkstack(L,LUA_MINSTACK,NULL);
    if (index < 0) {
        index = lua_gettop(L) + index + 1;
    }
    int array_size = append_table_array(L, bf, index, depth);
    append_table_hash(L, bf, index, depth, array_size);
}

static void
pack_one(lua_State *L, struct buffer *b, int index, int depth) {
    if (depth > MAX_DEPTH) {
        buffer_free(b);
        luaL_error(L, "serialize can't pack too depth table");
    }
    int type = lua_type(L,index);
    switch(type) {
    case LUA_TNIL:
        append_nil(b);
        break;
    case LUA_TNUMBER: {
        if (lua_isinteger(L, index)) {
            lua_Integer x = lua_tointeger(L,index);
            append_integer(b, x);
        } else {
            lua_Number n = lua_tonumber(L,index);
            append_real(b,n);
        }
        break;
    }
    case LUA_TBOOLEAN:
        append_boolean(b, lua_toboolean(L,index));
        break;
    case LUA_TSTRING: {
        size_t sz = 0;
        const char *str = lua_tolstring(L,index,&sz);
        append_string(b, str, (int)sz);
        break;
    }
    case LUA_TTABLE: {
        if (index < 0) {
            index = lua_gettop(L) + index + 1;
        }
        pack_table(L, b, index, depth+1);
        break;
    }
    default:
        buffer_free(b);
        luaL_error(L, "Unsupport type %s to serialize", lua_typename(L, type));
    }
}

int to_bin(lua_State *L) {
    struct buffer bf;
    buffer_initialize(&bf, L);

    for (int i = 1; i <= lua_gettop(L); ++i) {
        pack_one(L, &bf, i, 0);
    }

    buffer_push_string(&bf);
    buffer_free(&bf);

    return 1;
}

struct reader {
    const char *buffer;
    int len;
    int ptr;
};

static void reader_init(struct reader *rd, const char *buffer, int size) {
    rd->buffer = buffer;
    rd->len = size;
    rd->ptr = 0;
}

static const void *reader_read(struct reader *rd, int size) {
    if (rd->len < size)
        return NULL;

    int ptr = rd->ptr;
    rd->ptr += size;
    rd->len -= size;
    return rd->buffer + ptr;
}

static inline void
invalid_stream_line(lua_State *L, struct reader *rd, int line) {
    luaL_error(L, "Invalid serialize stream %d (line:%d)", rd->ptr, line);
}

#define invalid_stream(L,rd) invalid_stream_line(L,rd,__LINE__)

static lua_Integer
get_integer(lua_State *L, struct reader *rd, int cookie) {
    switch (cookie) {
    case TYPE_NUMBER_ZERO:
        return 0;
    case TYPE_NUMBER_BYTE: {
        uint8_t n = 0;
        const uint8_t *pn = reader_read(rd, sizeof(n));
        if (pn == NULL)
            invalid_stream(L,rd);
        n = *pn;
        return n;
    }
    case TYPE_NUMBER_WORD: {
        uint16_t n = 0;
        const uint16_t *pn = reader_read(rd, sizeof(n));
        if (pn == NULL)
            invalid_stream(L,rd);
        memcpy(&n, pn, sizeof(n));
        CONVERT(n);
        return n;
    }
    case TYPE_NUMBER_DWORD: {
        int32_t n = 0;
        const int32_t *pn = reader_read(rd, sizeof(n));
        if (pn == NULL)
            invalid_stream(L,rd);
        memcpy(&n, pn, sizeof(n));
        CONVERT(n);
        return n;
    }
    case TYPE_NUMBER_QWORD: {
        int64_t n=0;
        const int64_t *pn = reader_read(rd, sizeof(n));
        if (pn == NULL)
            invalid_stream(L,rd);
        memcpy(&n, pn, sizeof(n));
        CONVERT(n);
        return n;
    }
    default:
        invalid_stream(L,rd);
        return 0;
    }
}

static double
get_real(lua_State *L, struct reader *rd) {
    double n = 0;
    const double *pn = reader_read(rd, sizeof(n));
    if (pn == NULL)
        invalid_stream(L,rd);
    memcpy(&n, pn, sizeof(n));
    return n;
}

static void
get_buffer(lua_State *L, struct reader *rd, int len) {
    const char *p = reader_read(rd, len);
    lua_pushlstring(L, p, len);
}

static void unpack_one(lua_State *L, struct reader *rd);

static void
unpack_table(lua_State *L, struct reader *rd, int array_size) {
    if (array_size == MAX_COOKIE-1) {
        uint8_t type = 0;
        const uint8_t *t = reader_read(rd, sizeof(type));
        if (t == NULL) {
            invalid_stream(L,rd);
        }
        type = *t;
        int cookie = type >> 3;
        if ((type & 7) != TYPE_NUMBER || cookie == TYPE_NUMBER_REAL) {
            invalid_stream(L,rd);
        }
        array_size = get_integer(L, rd, cookie);
    }
    luaL_checkstack(L,LUA_MINSTACK,NULL);
    lua_createtable(L,array_size,0);
    int i;
    for (i=1;i<=array_size;i++) {
        unpack_one(L,rd);
        lua_rawseti(L,-2,i);
    }
    for (;;) {
        unpack_one(L,rd);
        if (lua_isnil(L,-1)) {
            lua_pop(L,1);
            return;
        }
        unpack_one(L,rd);
        lua_rawset(L,-3);
    }
}

static void
push_value(lua_State *L, struct reader *rd, int type, int cookie) {
    switch(type) {
    case TYPE_NIL:
        lua_pushnil(L);
        break;
    case TYPE_BOOLEAN:
        lua_pushboolean(L,cookie);
        break;
    case TYPE_NUMBER:
        if (cookie == TYPE_NUMBER_REAL) {
            lua_pushnumber(L,get_real(L,rd));
        } else {
            lua_pushinteger(L, get_integer(L, rd, cookie));
        }
        break;
    case TYPE_SHORT_STRING:
        get_buffer(L,rd,cookie);
        break;
    case TYPE_LONG_STRING: {
        if (cookie == 2) {
            const uint16_t *plen = reader_read(rd, 2);
            if (plen == NULL) {
                invalid_stream(L,rd);
            }
            uint16_t n;
            memcpy(&n, plen, sizeof(n));
            CONVERT(n);
            get_buffer(L,rd,n);
        } else {
            if (cookie != 4) {
                invalid_stream(L,rd);
            }
            const uint32_t *plen = reader_read(rd, 4);
            if (plen == NULL) {
                invalid_stream(L,rd);
            }
            uint32_t n;
            memcpy(&n, plen, sizeof(n));
            CONVERT(n);
            get_buffer(L,rd,n);
        }
        break;
    }
    case TYPE_TABLE: {
        unpack_table(L,rd,cookie);
        break;
    }
    default: {
        invalid_stream(L,rd);
        break;
    }
    }
}

static void
unpack_one(lua_State *L, struct reader *rd) {
    const uint8_t *t = reader_read(rd, sizeof(uint8_t));
    if (t==NULL) {
        invalid_stream(L, rd);
    }
    push_value(L, rd, *t & 0x7, *t >> 3);
}

int from_bin(lua_State *L) {
    size_t len;
    const char *buffer = luaL_checklstring(L, 1, &len);

    struct reader rd;
    reader_init(&rd, buffer, len);
    for (int i = 0;; ++i) {
        if (i % 16 == 15) {
            lua_checkstack(L, i);
        }
        const uint8_t *t = reader_read(&rd, sizeof(uint8_t));
        if (!t) break;
        push_value(L, &rd, *t & 0x7, *t >> 3);
    }

    return lua_gettop(L) - 1;
}
