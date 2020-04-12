#include <lauxlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include "buffer.h"

#define MAX_DEPTH 32

static const char *char2escape[256] = {
    "\\u0000", "\\u0001", "\\u0002", "\\u0003",
    "\\u0004", "\\u0005", "\\u0006", "\\u0007",
    "\\b", "\\t", "\\n", "\\u000b",
    "\\f", "\\r", "\\u000e", "\\u000f",
    "\\u0010", "\\u0011", "\\u0012", "\\u0013",
    "\\u0014", "\\u0015", "\\u0016", "\\u0017",
    "\\u0018", "\\u0019", "\\u001a", "\\u001b",
    "\\u001c", "\\u001d", "\\u001e", "\\u001f",
    NULL, NULL, "\\\"", NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\/",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, "\\\\", NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, "\\u007f",
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL,
};

inline static bool
is_name(const char *str, size_t len) {
    if (len == 0) return false;
    char c = *str;
    if (c != '_' && !isalpha(c))
        return false;
    for (size_t i = 1; i < len; ++i) {
        c = str[i];
        if (c != '_' && !isalpha(c) && !isdigit(c))
            return false;
    }
    return true;
}

inline static void
append_escape_string(struct buffer *bf, const char *str, size_t len) {
    for (int i = 0; i < len; ++i) {
        const char *esc = char2escape[(unsigned char)str[i]];
        if (esc)
            buffer_append_str(bf, esc);
        else
            buffer_append_char(bf, str[i]);
    }
}

static void
_serialize(lua_State *L, int idx, struct buffer *bf, bool is_key, int depth) {
    if (depth > MAX_DEPTH) {
        buffer_free(bf);
        luaL_error(L, "serialize can't pack too depth table");
    }

    int type = lua_type(L, idx);
    char numbuff[64] = {0};
    switch(type) {
    case LUA_TNIL:
        buffer_append_lstr(bf, "nil", 3);
        break;
    case LUA_TNUMBER: {
        if (is_key) buffer_append_char(bf, '[');
#if LUA_VERSION_NUM < 503
        int len = lua_number2str(numbuff, lua_tonumber(L, idx));
        buffer_append_lstr(bf, numbuff, len);
#else
        if (lua_isinteger(L, idx)) {
            int len = lua_integer2str(numbuff, sizeof(numbuff), lua_tointeger(L, idx));
            buffer_append_lstr(bf, numbuff, len);
        } else {
            int len = lua_number2str(numbuff, sizeof(numbuff), lua_tonumber(L, idx));
            buffer_append_lstr(bf, numbuff, len);
        }
#endif
        if (is_key) buffer_append_lstr(bf, "]=", 2);
        break;
    }
    case LUA_TBOOLEAN:
        if (is_key) buffer_append_char(bf, '[');
        if (lua_toboolean(L, idx))
            buffer_append_lstr(bf, "true", 4);
        else
            buffer_append_lstr(bf, "false", 5);
        if (is_key) buffer_append_lstr(bf, "]=", 2);
        break;
    case LUA_TSTRING: {
        size_t len;
        const char *str = lua_tolstring(L, idx, &len);
        if (is_key) {
            if (is_name(str, len)) {
                buffer_append_lstr(bf, str, len);
                buffer_append_char(bf, '=');
            } else {
                buffer_append_lstr(bf, "[\"", 2);
                append_escape_string(bf, str, len);
                buffer_append_lstr(bf, "\"]=", 3);
            }
        } else {
            buffer_append_char(bf, '"');
            append_escape_string(bf, str, len);
            buffer_append_char(bf, '"');
        }
        break;
    }
    case LUA_TTABLE:
        luaL_checkstack(L, LUA_MINSTACK, NULL);
        if (is_key) buffer_append_char(bf, '[');
        lua_pushnil(L);
        buffer_append_char(bf, '{');
        int first = 1;
        while (lua_next(L, idx)) {
            if (first)
                first = 0;
            else
                buffer_append_char(bf, ',');
            int top = lua_gettop(L);
            _serialize(L, top - 1, bf, true, depth + 1);
            _serialize(L, top, bf, false, depth + 1);
            lua_pop(L, 1);
        }
        buffer_append_char(bf, '}');
        if (is_key) buffer_append_lstr(bf, "]=", 2);
        break;
    default:
        buffer_free(bf);
        luaL_error(L, "Bad type: %s", lua_typename(L, type));
    }
}

int to_txt(lua_State *L) {
    struct buffer bf;
    buffer_initialize(&bf, L);

    for (int i = 1; i <= lua_gettop(L); ++i) {
        if (i != 1)
            buffer_append_char(&bf, ',');
        _serialize(L, i, &bf, false, 0);
    }

    buffer_push_string(&bf);
    buffer_free(&bf);

    return 1;
}
