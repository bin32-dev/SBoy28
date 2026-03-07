#include "lua_syscalls.h"

#include "lua.h"
#include "lauxlib.h"

static lua_os_api_t g_os_api;

int lua_fs_open(lua_State* L, const lua_os_api_t* api);
int lua_window_create(lua_State* L, const lua_os_api_t* api);
int lua_process_spawn(lua_State* L, const lua_os_api_t* api);

void lua_syscalls_set_api(const lua_os_api_t* api)
{
    if (!api) {
        return;
    }

    g_os_api = *api;
}

static int lua_os_print(lua_State* L)
{
    const char* message = luaL_checkstring(L, 1);

    if (!g_os_api.print) {
        // TODO(OS): wire console print syscall for Lua.
        return 0;
    }

    (void)g_os_api.print(message);
    return 0;
}

static int lua_os_window_create(lua_State* L)
{
    return lua_window_create(L, &g_os_api);
}

static int lua_os_fs_open(lua_State* L)
{
    return lua_fs_open(L, &g_os_api);
}

static int lua_os_process_spawn(lua_State* L)
{
    return lua_process_spawn(L, &g_os_api);
}

static int lua_os_memory_stats(lua_State* L)
{
    if (!g_os_api.memory_stats) {
        // TODO(OS): implement memory manager stats syscall wrapper.
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, g_os_api.memory_stats());
    return 1;
}

static int lua_os_console_write(lua_State* L)
{
    const char* message = luaL_checkstring(L, 1);

    if (!g_os_api.console_write) {
        // TODO(OS): map terminal output to Lua console API.
        return 0;
    }

    (void)g_os_api.console_write(message);
    return 0;
}

static int lua_os_input_poll(lua_State* L)
{
    if (!g_os_api.input_poll) {
        // TODO(OS): expose keyboard/mouse input polling to Lua.
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, g_os_api.input_poll());
    return 1;
}

int lua_bind_register_syscalls(lua_State* L)
{
    lua_newtable(L);

    lua_pushcfunction(L, lua_os_print);
    lua_setfield(L, -2, "print");

    lua_pushcfunction(L, lua_os_window_create);
    lua_setfield(L, -2, "window_create");

    lua_pushcfunction(L, lua_os_fs_open);
    lua_setfield(L, -2, "fs_open");

    lua_pushcfunction(L, lua_os_process_spawn);
    lua_setfield(L, -2, "process_spawn");

    lua_pushcfunction(L, lua_os_memory_stats);
    lua_setfield(L, -2, "memory_stats");

    lua_pushcfunction(L, lua_os_console_write);
    lua_setfield(L, -2, "console_write");

    lua_pushcfunction(L, lua_os_input_poll);
    lua_setfield(L, -2, "input_poll");

    lua_setglobal(L, "os");
    return 0;
}
