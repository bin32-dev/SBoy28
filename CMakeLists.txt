cmake_minimum_required(VERSION 3.16)

# ----------------------
# Toolchain and Project
# ----------------------
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

# ----------------------
# Compile flags
# ----------------------
set(COMMON_C_FLAGS -m32 -std=gnu99 -ffreestanding -fno-stack-protector -fno-pic -fno-builtin -O2 -Wall -Wextra)
set(COMMON_ASM_FLAGS -m32)
set(COMMON_LINK_FLAGS -m32 -nostdlib -nodefaultlibs -nostartfiles -static)

include_directories(include)

# ----------------------
# Sources
# ----------------------
file(GLOB KERNEL_CORE_SOURCES CONFIGURE_DEPENDS
    src/kernel/core/*.c 
    src/kernel/core/*.s 
    src/ui/*.c 
    src/common/*.c
)
list(REMOVE_ITEM KERNEL_CORE_SOURCES src/kernel/core/power.c)

file(GLOB KERNEL_DRIVER_SOURCES CONFIGURE_DEPENDS
    src/drivers/*.c
    src/drivers/mouse/src/*.c
    src/drivers/thread/src/*.c
    src/drivers/thread/asm/*.s
    src/drivers/thread/asm/*.S
)

set(KERNEL_SOURCES 
    ${KERNEL_CORE_SOURCES} 
    ${KERNEL_DRIVER_SOURCES} 
    src/kernel/core/power.c 
    apps/OS/OS.c 
    apps/OS/API/Grapich/gui.c 
    apps/OS/API/Grapich/windows.c
)

set(BOOT_SOURCE src/boot/boot.s)
set(BOOTSECTOR_SOURCE src/boot/bootsector.s)
set(LINKER_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/src/kernel/core/linker.ld)

# ----------------------
# Bootsector
# ----------------------
add_executable(bootsector.elf ${BOOTSECTOR_SOURCE})
target_compile_options(bootsector.elf PRIVATE ${COMMON_ASM_FLAGS})
target_link_options(bootsector.elf PRIVATE ${COMMON_LINK_FLAGS} -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/src/boot/bootsector.ld -Wl,--oformat=elf32-i386)
add_custom_command(TARGET bootsector.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary bootsector.elf bootsector.bin
    COMMAND ${CMAKE_COMMAND} -E copy bootsector.elf ${CMAKE_BINARY_DIR}/bootsector.elf
)

# ----------------------
# Kernel
# ----------------------
add_executable(kernel.elf ${BOOT_SOURCE} ${KERNEL_SOURCES})
target_compile_options(kernel.elf PRIVATE ${COMMON_C_FLAGS} $<$<COMPILE_LANGUAGE:ASM>:${COMMON_ASM_FLAGS}>)
target_link_options(kernel.elf PRIVATE ${COMMON_LINK_FLAGS} -Wl,-T,${LINKER_SCRIPT} -Wl,--oformat=elf32-i386)
add_custom_command(TARGET kernel.elf POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary kernel.elf kernel.bin
    COMMAND ${CMAKE_COMMAND} -E copy kernel.elf ${CMAKE_BINARY_DIR}/kernel.elf
)

# ----------------------
# Disk Image
# ----------------------
set(IMG_FILE ${CMAKE_BINARY_DIR}/SBoy28.img)
set(IMG_SIZE_KB 32768)  # 32MB disk image

if(WIN32)
    # Windows
    add_custom_target(SBoy28.img ALL
        COMMAND cmd /c "copy /b bootsector.bin+kernel.bin SBoy28.img"
        COMMAND python3 "${CMAKE_CURRENT_SOURCE_DIR}/tools/pad_img.py" SBoy28.img ${IMG_SIZE_KB}
        COMMAND ${CMAKE_COMMAND} -E copy SBoy28.img ${CMAKE_BINARY_DIR}/SBoy28.img
        DEPENDS bootsector.elf kernel.elf
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    )
else()
    # Linux / Unix
    add_custom_target(SBoy28.img ALL
        COMMAND cat bootsector.bin kernel.bin > ${IMG_FILE}
        COMMAND dd if=/dev/zero bs=1K count=${IMG_SIZE_KB} >> ${IMG_FILE} 2>/dev/null || true
        DEPENDS bootsector.elf kernel.elf
    )
endif()

# ----------------------
# Run in QEMU
# ----------------------
add_custom_target(run
    COMMAND qemu-system-i386 -drive format=raw,file=${IMG_FILE}
    DEPENDS SBoy28.img
)