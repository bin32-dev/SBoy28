cmake_minimum_required(VERSION 3.16)

project(lua
        VERSION 5.4
        LANGUAGES C)

# =========================
# C standard
# =========================

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# =========================
# Source Files
# =========================

set(LUA_CORE
        lapi.c
        lcode.c
        lctype.c
        ldebug.c
        ldo.c
        ldump.c
        lfunc.c
        lgc.c
        llex.c
        lmem.c
        lobject.c
        lopcodes.c
        lparser.c
        lstate.c
        lstring.c
        ltable.c
        ltm.c
        lundump.c
        lvm.c
        lzio.c
        ltests.c
        )

set(LUA_AUX
        lauxlib.c
        )

set(LUA_LIB
        lbaselib.c
        ldblib.c
        liolib.c
        lmathlib.c
        loslib.c
        ltablib.c
        lstrlib.c
        lutf8lib.c
        loadlib.c
        lcorolib.c
        linit.c
        )

set(LUA_INTERPRETER
        lua.c
        )

# =========================
# Lua Static Library
# =========================

add_library(lua STATIC
        ${LUA_CORE}
        ${LUA_AUX}
        ${LUA_LIB}
        )

target_include_directories(lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# =========================
# Compiler Warnings
# =========================

target_compile_options(lua PRIVATE
        -Wall
        -Wextra
        -Wshadow
        -Wundef
        -Wwrite-strings
        -Wredundant-decls
        -Wdouble-promotion
        -Wmissing-declarations
        -Wconversion
        )

# GCC only warnings
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(lua PRIVATE
            -Wlogical-op
            -Wno-aggressive-loop-optimizations
            )
endif()

# Lua Linux features
if (UNIX AND NOT APPLE)
    target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
endif()

# =========================
# Lua Interpreter
# =========================

add_executable(lua_cli
        ${LUA_INTERPRETER}
        )

target_link_libraries(lua_cli PRIVATE lua)

# =========================
# Linux system libraries
# =========================

if (UNIX)
    target_link_libraries(lua_cli PRIVATE m dl)
endif()