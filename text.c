#include <lauxlib.h>
#include <stdint.h>
#include <string.h>
#include "buffer.h"

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

static void _serialize(lua_State *L, int idx, struct buffer *bf) {
    int type = lua_type(L, idx);
    char numbuff[64] = {0};
    switch(type) {
    case LUA_TNIL:
        buffer_append_lstr(bf, "nil", 3);
        break;
    case LUA_TNUMBER:
#if LUA_VERSION_NUM < 503
        {
            int len = lua_number2str(numbuff, lua_tonumber(L, idx));
            buffer_append_lstr(bf, numbuff, len);
            break;
        }
#else
        if (lua_isinteger(L, idx)) {
            int len = lua_integer2str(numbuff, sizeof(numbuff), lua_tointeger(L, idx));
            buffer_append_lstr(bf, numbuff, len);
        } else {
            int len = lua_number2str(numbuff, sizeof(numbuff), lua_tonumber(L, idx));
            buffer_append_lstr(bf, numbuff, len);
        }
        break;
#endif
    case LUA_TBOOLEAN:
        if (lua_toboolean(L, idx))
            buffer_append_lstr(bf, "true", 4);
        else
            buffer_append_lstr(bf, "false", 5);
        break;
    case LUA_TSTRING: {
        size_t len;
        const char *str = lua_tolstring(L, idx, &len);
        buffer_append_char(bf, '"');
        for (int i = 0; i < len; ++i) {
            const char *esc = char2escape[(unsigned char)str[i]];
            if (esc)
                buffer_append_str(bf, esc);
            else
                buffer_append_char(bf, str[i]);
        }
        buffer_append_char(bf, '"');
        break;
    }
    case LUA_TTABLE:
        lua_pushnil(L);
        buffer_append_char(bf, '{');
        int first = 1;
        while (lua_next(L, idx)) {
            if (first)
                first = 0;
            else
                buffer_append_char(bf, ',');
            int top = lua_gettop(L);
            buffer_append_char(bf, '[');
            _serialize(L, top - 1, bf);
            buffer_append_lstr(bf, "]=", 2);
            _serialize(L, top, bf);
            lua_pop(L, 1);
        }
        buffer_append_char(bf, '}');
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
        _serialize(L, i, &bf);
    }

    struct block *p = bf.head;
    luaL_Buffer buffer;

#if LUA_VERSION_NUM < 503
    luaL_buffinit(L, &buffer);
#else
    size_t size = buffer_size(&bf);
    luaL_buffinitsize(L, &buffer, size);
#endif
    while (p) {
        luaL_addlstring(&buffer, p->data, p->p);
        p = p->next;
    }

    luaL_pushresult(&buffer);
    buffer_free(&bf);

    return 1;
}
