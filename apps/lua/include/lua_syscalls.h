#ifndef APPS_LUA_SYSCALLS_H
#define APPS_LUA_SYSCALLS_H

struct lua_State;

typedef struct lua_os_api {
    int (*print)(const char* message);
    int (*window_create)(int width, int height, const char* title);
    int (*fs_open)(const char* path);
    int (*process_spawn)(const char* program);
    int (*memory_stats)(void);
    int (*console_write)(const char* message);
    int (*input_poll)(void);
} lua_os_api_t;

void lua_syscalls_set_api(const lua_os_api_t* api);
int lua_bind_register_syscalls(struct lua_State* L);

#endif /* APPS_LUA_SYSCALLS_H */
