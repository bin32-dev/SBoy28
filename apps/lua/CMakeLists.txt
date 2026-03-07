cmake_minimum_required(VERSION 3.16)

project(sboy28_lua_module LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

set(LUA_MODULE_HEADERS
    include/lua_app.h
    include/lua_runtime.h
    include/lua_syscalls.h
)

set(LUA_MODULE_SOURCES
    src/runtime/lua_runtime.c
    src/runtime/lua_loader.c
    src/bindings/lua_syscalls.c
    src/bindings/lua_fs.c
    src/bindings/lua_window.c
    src/bindings/lua_process.c
    src/core/lua_app.c
    src/core/lua_vm.c
    src/utils/lua_utils.c
)

add_library(lua_app_module STATIC
    ${LUA_MODULE_HEADERS}
    ${LUA_MODULE_SOURCES}
)

target_include_directories(lua_app_module
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_options(lua_app_module PRIVATE
    -Wall
    -Wextra
    -Wshadow
)


# Integrate with top-level OS build when this directory is included as a subproject.
if (TARGET kernel.elf)
    target_link_libraries(kernel.elf PRIVATE lua_app_module)
endif()
