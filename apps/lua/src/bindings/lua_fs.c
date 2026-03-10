#include "lua.h"
#include "lauxlib.h"

#include "lua_syscalls.h"

#include <stddef.h>

#define LUA_FS_MAX_BUFFER 1024

int lua_fs_open(lua_State* L, const lua_os_api_t* api)
{
    const char* path = luaL_checkstring(L, 1);

    if (!api || !api->fs_open) {
        lua_pushinteger(L, -1);
        return 1;
    }

    lua_pushinteger(L, api->fs_open(path));
    return 1;
}

int lua_fs_read(lua_State* L, const lua_os_api_t* api)
{
    const char* path = luaL_checkstring(L, 1);
    int fd;
    char buffer[LUA_FS_MAX_BUFFER + 1];
    int bytes;

    if (!api || !api->fs_open || !api->fs_read || !api->fs_close) {
        lua_pushnil(L);
        return 1;
    }

    fd = api->fs_open(path);
    if (fd < 0) {
        lua_pushnil(L);
        return 1;
    }

    bytes = api->fs_read(fd, buffer, LUA_FS_MAX_BUFFER);
    (void)api->fs_close(fd);

    if (bytes < 0) {
        lua_pushnil(L);
        return 1;
    }

    buffer[bytes] = '\0';
    lua_pushlstring(L, buffer, (size_t)bytes);
    return 1;
}

int lua_fs_write(lua_State* L, const lua_os_api_t* api)
{
    const char* path = luaL_checkstring(L, 1);
    size_t length = 0;
    const char* data = luaL_checklstring(L, 2, &length);
    int fd;
    int bytes;

    if (!api || !api->fs_open || !api->fs_write || !api->fs_close) {
        lua_pushboolean(L, 0);
        return 1;
    }

    fd = api->fs_open(path);
    if (fd < 0) {
        lua_pushboolean(L, 0);
        return 1;
    }

    bytes = api->fs_write(fd, data, (int)length);
    (void)api->fs_close(fd);

    lua_pushboolean(L, bytes >= 0 && (size_t)bytes == length);
    return 1;
}

int lua_fs_exists(lua_State* L, const lua_os_api_t* api)
{
    const char* path = luaL_checkstring(L, 1);

    if (!api || !api->fs_exists) {
        lua_pushboolean(L, 0);
        return 1;
    }

    lua_pushboolean(L, api->fs_exists(path));
    return 1;
}
