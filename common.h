#define MAX_DEPTH 32

#if LUA_VERSION_NUM < 502
#define lua_rawlen lua_objlen
#endif

#if LUA_VERSION_NUM < 503

static int
lua_isinteger(lua_State *L, int index) {
    int32_t x = (int32_t)lua_tointeger(L,index);
    lua_Number n = lua_tonumber(L,index);
    return ((lua_Number)x==n);
}

#endif
