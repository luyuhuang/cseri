#include <lua.h>
#include <lauxlib.h>

int to_bin(lua_State *L);
int from_bin(lua_State *L);
int to_txt(lua_State *L);

LUA_API int luaopen_cseri(lua_State *L) {
    luaL_Reg l[] = {
        {"tobin", to_bin},
        {"frombin", from_bin},
        {"totxt", to_txt},
        {NULL, NULL}
    };
#if LUA_VERSION_NUM < 502
    luaL_register(L, "cseri", l);
#else
    luaL_newlib(L, l);
#endif
    return 1;
}
