#include "lua_runtime.h"

#include "lauxlib.h"
#include "lua.h"
#include "lualib.h"

#include "lua_syscalls.h"

#include "common/utils.h"
#include "drivers/filesystem.h"

#define LUA_STARTUP_SCRIPT "2:/lua/init.lua"

static int lua_os_print_impl(const char* message)
{
    if (!message) return -1;
    print_string(message);
    print_new_line();
    return 0;
}

static int lua_os_console_write_impl(const char* message)
{
    if (!message) return -1;
    print_string(message);
    return 0;
}

static int lua_os_fs_exists_impl(const char* path)
{
    int fd;
    if (!path) return 0;
    fd = fs_open(path);
    if (fd < 0) return 0;
    (void)fs_close(fd);
    return 1;
}

static void lua_runtime_install_os_api(void)
{
    lua_os_api_t api;
    api.print = lua_os_print_impl;
    api.window_create = 0;
    api.fs_open = fs_open;
    api.fs_read = fs_read;
    api.fs_write = fs_write;
    api.fs_close = fs_close;
    api.fs_exists = lua_os_fs_exists_impl;
    api.process_spawn = 0;
    api.memory_stats = 0;
    api.console_write = lua_os_console_write_impl;
    api.input_poll = 0;
    lua_syscalls_set_api(&api);
}

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
    lua_runtime_install_os_api();

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

int lua_runtime_execute_startup(void)
{
    return lua_runtime_execute(LUA_STARTUP_SCRIPT);
}

void lua_runtime_shutdown(void)
{
    if (!g_lua_state) {
        return;
    }

    lua_close(g_lua_state);
    g_lua_state = 0;
}
