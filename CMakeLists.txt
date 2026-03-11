cmake_minimum_required(VERSION 3.16)

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR i686)
set(CMAKE_C_COMPILER i686-elf-gcc)
set(CMAKE_ASM_COMPILER i686-elf-gcc)
set(CMAKE_LINKER i686-elf-ld)
set(CMAKE_OBJCOPY i686-elf-objcopy)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

project(SBoy28 VERSION 0.1 LANGUAGES C ASM)
enable_language(ASM)

set(CMAKE_EXECUTABLE_SUFFIX "")

find_program(SBOY28_PYTHON NAMES python3 python REQUIRED)

# Optional API documentation generation with Doxygen.
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in ${CMAKE_BINARY_DIR}/Doxyfile @ONLY)
    add_custom_target(docs
            COMMAND ${DOXYGEN_EXECUTABLE} ${CMAKE_BINARY_DIR}/Doxyfile
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generating API documentation with Doxygen"
            VERBATIM
            )
else()
    add_custom_target(docs
            COMMAND ${CMAKE_COMMAND} -E echo "Doxygen not found. Install doxygen to generate API docs."
            )
endif()


# Compile flags
set(COMMON_C_FLAGS -m32 -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -fno-builtin -O2 -Wall -Wextra)
set(COMMON_ASM_FLAGS -m32)
set(COMMON_LINK_FLAGS -m32 -nostdlib -nodefaultlibs -nostartfiles -static)

include_directories(include)
include_directories(src/drivers/fat/source)

add_subdirectory(apps/lua)

# Sources

set(FATFS_SOURCES
    src/drivers/fat/source/ff.c
    src/drivers/fat/source/ffsystem.c
    src/drivers/fat/diskio.c
)
file(GLOB KERNEL_CORE_SOURCES CONFIGURE_DEPENDS
    src/kernel/core/*.c 
    src/kernel/core/*.s 
    src/ui/*.c 
    src/common/*.c
    src/libc/*.c
)
list(REMOVE_ITEM KERNEL_CORE_SOURCES src/kernel/core/power.c)

file(GLOB KERNEL_DRIVER_SOURCES CONFIGURE_DEPENDS
    src/drivers/*.c
    src/drivers/mouse/src/*.c
    src/drivers/thread/src/*.c
)

# Keep thread assembly sources explicit so critical context switch code is
# always included across toolchains/generators.
set(KERNEL_THREAD_ASM_SOURCES
    src/drivers/thread/asm/context_switch.S
)

set(KERNEL_SOURCES
    ${KERNEL_CORE_SOURCES}
    ${KERNEL_DRIVER_SOURCES}
    ${KERNEL_THREAD_ASM_SOURCES}
    ${FATFS_SOURCES}
    src/kernel/core/power.c
    apps/OS/OS.c
    apps/OS/API/Grapich/gui.c
    apps/OS/API/Grapich/windows.c
)
set(BOOT_SOURCE src/boot/boot.s)
set(BOOTSECTOR_SOURCE src/boot/bootsector.s)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel/core/linker.ld)

# Bootsector
add_executable(bootsector.elf ${BOOTSECTOR_SOURCE})
target_compile_options(bootsector.elf PRIVATE ${COMMON_ASM_FLAGS})
target_link_options(bootsector.elf PRIVATE ${COMMON_LINK_FLAGS} -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/src/boot/bootsector.ld -Wl,--oformat=elf32-i386)
add_custom_command(TARGET bootsector.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary bootsector.elf bootsector.bin
        COMMAND ${CMAKE_COMMAND} -E copy bootsector.elf ${CMAKE_SOURCE_DIR}/build/bootsector.elf
        )

# Kernel
add_executable(kernel.elf ${BOOT_SOURCE} ${KERNEL_SOURCES})
target_link_libraries(kernel.elf PRIVATE lua_app_module)
target_compile_options(kernel.elf PRIVATE ${COMMON_C_FLAGS})
target_compile_options(kernel.elf PRIVATE $<$<COMPILE_LANGUAGE:ASM>:${COMMON_ASM_FLAGS}>)
target_link_options(kernel.elf PRIVATE ${COMMON_LINK_FLAGS} -Wl,-T,${LINKER_SCRIPT} -Wl,--oformat=elf32-i386)
add_custom_command(TARGET kernel.elf POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O binary kernel.elf kernel.bin
        COMMAND ${CMAKE_COMMAND} -E copy kernel.elf ${CMAKE_SOURCE_DIR}/build/kernel.elf
        )

# Disk Image
set(IMG_FILE ${CMAKE_BINARY_DIR}/SBoy28.img)
set(DATA_IMG_FILE ${CMAKE_BINARY_DIR}/data.img)
set(APPS_IMG_FILE ${CMAKE_BINARY_DIR}/apps.img)
set(DATA_IMG_SIZE 33554432)
set(APPS_IMG_SIZE 33554432)
if(CMAKE_HOST_WIN32)
    add_custom_target(SBoy28.img ALL
            COMMAND cmd /c "copy /b bootsector.bin + kernel.bin SBoy28.img"
            COMMAND cmd /c ${SBOY28_PYTHON} \"${CMAKE_CURRENT_SOURCE_DIR}/tools/pad_img.py\" SBoy28.img 32768
            COMMAND ${CMAKE_COMMAND} -E copy SBoy28.img ${CMAKE_SOURCE_DIR}/build/SBoy28.img
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS bootsector.elf kernel.elf
            )
else()
    add_custom_target(SBoy28.img ALL
            COMMAND cat bootsector.bin kernel.bin > ${IMG_FILE}
            COMMAND ${CMAKE_COMMAND} -E copy ${IMG_FILE} ${CMAKE_SOURCE_DIR}/build/SBoy28.img
            DEPENDS bootsector.elf kernel.elf
            )
endif()

# Auxiliary disk images used by the kernel filesystem layer.
add_custom_target(data.img ALL
        COMMAND ${CMAKE_COMMAND} -E touch ${DATA_IMG_FILE}
        COMMAND ${SBOY28_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/pad_img.py ${DATA_IMG_FILE} ${DATA_IMG_SIZE}
        COMMAND ${CMAKE_COMMAND} -E copy ${DATA_IMG_FILE} ${CMAKE_SOURCE_DIR}/build/data.img
        )

add_custom_target(apps.img ALL
        COMMAND ${CMAKE_COMMAND} -E touch ${APPS_IMG_FILE}
        COMMAND ${SBOY28_PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/tools/pad_img.py ${APPS_IMG_FILE} ${APPS_IMG_SIZE}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/apps_disk/lua
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/apps/lua/scripts/init.lua ${CMAKE_BINARY_DIR}/apps_disk/lua/init.lua
        COMMAND ${CMAKE_COMMAND} -E copy ${CMAKE_SOURCE_DIR}/apps/lua/scripts/test.lua ${CMAKE_BINARY_DIR}/apps_disk/lua/test.lua
        COMMAND ${CMAKE_COMMAND} -E copy ${APPS_IMG_FILE} ${CMAKE_SOURCE_DIR}/build/apps.img
        )

# Package all runtime images into one distributable archive.
add_custom_target(images.bundle
        COMMAND ${CMAKE_COMMAND} -E tar cfv ${CMAKE_BINARY_DIR}/SBoy28-images.tar --format=gnutar
            ${IMG_FILE}
            ${DATA_IMG_FILE}
            ${APPS_IMG_FILE}
        DEPENDS SBoy28.img data.img apps.img
        )

# Run
add_custom_target(run
        COMMAND qemu-system-i386
        -drive format=raw,file=${IMG_FILE}
        -drive format=raw,file=${DATA_IMG_FILE}
        -drive format=raw,file=${APPS_IMG_FILE}
        DEPENDS SBoy28.img data.img apps.img
        )
