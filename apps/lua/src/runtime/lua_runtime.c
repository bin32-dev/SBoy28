#include "lua_runtime.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "lua_syscalls.h"

int lua_loader_load_script(lua_State* L, const char* script_path);

static lua_State* g_lua_state;

int lua_runtime_init(void)
{
    if (g_lua_state) {
        return 0;
    }

    g_lua_state = luaL_newstate();
    if (!g_lua_state) {
        return -1;
    }

    luaL_openlibs(g_lua_state);

    if (lua_bind_register_syscalls(g_lua_state) != 0) {
        lua_close(g_lua_state);
        g_lua_state = 0;
        return -1;
    }

    return 0;
}

int lua_runtime_execute(const char* script_path)
{
    if (!script_path) {
        return -1;
    }

    if (!g_lua_state && lua_runtime_init() != 0) {
        return -1;
    }

    if (lua_loader_load_script(g_lua_state, script_path) != LUA_OK) {
        return -1;
    }

    if (lua_pcall(g_lua_state, 0, LUA_MULTRET, 0) != LUA_OK) {
        return -1;
    }

    return 0;
}

void lua_runtime_shutdown(void)
{
    if (!g_lua_state) {
        return;
    }

    lua_close(g_lua_state);
    g_lua_state = 0;
}
