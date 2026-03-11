#ifndef APPS_LUA_SYSCALLS_H
#define APPS_LUA_SYSCALLS_H

/**
 * @file lua_syscalls.h
 * @brief OS-to-Lua syscall bridge API.
 */

struct lua_State;

/**
 * @brief Function table exposed by the OS to Lua bindings.
 *
 * Any callback may be left as `0` if the feature is not available.
 */
typedef struct lua_os_api {
    /** @brief Print a line to the default output. */
    int (*print)(const char* message);
    /** @brief Create a window and return a handle or status code. */
    int (*window_create)(int width, int height, const char* title);
    /** @brief Open a file and return file descriptor or negative error. */
    int (*fs_open)(const char* path);
    /** @brief Read from an open file descriptor. */
    int (*fs_read)(int fd, void* buffer, int size);
    /** @brief Write to an open file descriptor. */
    int (*fs_write)(int fd, const void* buffer, int size);
    /** @brief Close an open file descriptor. */
    int (*fs_close)(int fd);
    /** @brief Check whether a file path exists. */
    int (*fs_exists)(const char* path);
    /** @brief Spawn a process/program by name or path. */
    int (*process_spawn)(const char* program);
    /** @brief Return memory statistics/status code. */
    int (*memory_stats)(void);
    /** @brief Write text to the console without forcing newline. */
    int (*console_write)(const char* message);
    /** @brief Poll input events and return status. */
    int (*input_poll)(void);
} lua_os_api_t;

/**
 * @brief Register/replace the active syscall table for Lua bindings.
 * @param api Pointer to a syscall table that stays valid while in use.
 */
void lua_syscalls_set_api(const lua_os_api_t* api);

/**
 * @brief Register syscall bindings into a Lua VM.
 * @param L Lua VM state.
 * @return 0 on success, non-zero on failure.
 */
int lua_bind_register_syscalls(struct lua_State* L);

#endif /* APPS_LUA_SYSCALLS_H */
