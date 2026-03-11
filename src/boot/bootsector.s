# =============================================================================
# SBoy28 OS Custom Bootsector (MBR)
# Loads the kernel from the second sector onwards and jumps to 32-bit PM.
# =============================================================================

.code16
.section .text
.global _start

_start:
    # Set up segments
    cli
    xor %ax, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %ss
    mov $0x7C00, %sp
    sti

    mov %dl, boot_drive     # Save boot drive number

    # Switch to VGA Mode 13h (320x200, 256 colors)
    mov $0x0013, %ax
    int $0x10

    # DEBUG: Draw a Red bar in Real Mode (top 10 rows)
    mov $0xA000, %ax
    mov %ax, %es
    xor %di, %di
    mov $1600, %cx     # 320 * 10 / 2 = 1600 words
    mov $0x0404, %ax   # Red color (index 4)
    rep stosw

    # Enable A20 Line (Fast A20)
    in $0x92, %al
    or $2, %al
    out %al, $0x92

    # Print loading message
    mov $msg_loading, %si
    call print_string

    # Reset Disk System
    xor %ax, %ax
    mov boot_drive, %dl
    int $0x13

    # Load kernel from Disk (Sector 2 onwards).
    # Read one sector at a time so we can safely advance CHS and retry
    # individual sectors on transient BIOS read failures.
    mov $0x1000, %ax
    mov %ax, %es
    xor %bx, %bx

    mov $60, %si            # Read 60 sectors (30KB)
    mov $0x00, %ch          # Cylinder 0
    mov $0x00, %dh          # Head 0
    mov $0x02, %cl          # Sector 2

load_kernel:
    mov $3, %di             # Retry counter (per sector)

read_sector_retry:
    mov $0x02, %ah          # BIOS read sectors
    mov $0x01, %al          # Read 1 sector
    mov boot_drive, %dl     # Drive number
    int $0x13
    jnc read_sector_ok

    # Error occurred: reset disk system, then retry this sector.
    xor %ax, %ax
    mov boot_drive, %dl
    int $0x13

    dec %di
    jnz read_sector_retry
    jmp disk_error

read_sector_ok:
    add $512, %bx
    dec %si
    jz load_success

    call advance_chs
    jmp load_kernel

load_success:

    # Transition to Protected Mode
    cli
    lgdt gdt_descriptor
    mov %cr0, %eax
    or $0x01, %eax
    mov %eax, %cr0

    # Far jump to clear prefetch queue and enter 32-bit mode
    ljmp $0x08, $init_pm

.code32
init_pm:
    # Set up segments for Protected Mode
    mov $0x10, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    mov %ax, %ss

    # DEBUG: Draw a Green bar in Protected Mode (rows 10-20)
    mov $0xA0000, %edi
    add $3200, %edi    # Offset 3200 (320 * 10)
    mov $3200, %ecx    # 320 * 10
    mov $0x02, %al     # Green (index 2)
    rep stosb

    # Jump to kernel entry (0x10000)
    jmp *kernel_exec_addr

# ---------------------------------------------------------
# Helpers
# ---------------------------------------------------------

.code16
print_string:
    mov $0x0E, %ah
    mov $0x000F, %bx      # BH=0 (page), BL=15 (white)
.loop:
    lodsb
    test %al, %al
    jz .done
    int $0x10
    jmp .loop
.done:
    ret

advance_chs:
    inc %cl
    cmp $19, %cl            # sectors 1..18
    jne .chs_done

    mov $1, %cl
    xor $1, %dh             # toggle head 0<->1
    jnz .chs_done

    inc %ch                 # wrapped head back to 0 => next cylinder
.chs_done:
    jne .done

    mov $1, %cl
    xor $1, %dh             # toggle head 0<->1
    jnz .done

    inc %ch                 # wrapped head back to 0 => next cylinder
.done:
    ret

disk_error:
    mov $msg_error, %si
    call print_string

    # Halt permanently without re-printing the message.
    # HLT can resume on interrupts, so disable them and idle-loop.
    cli
.disk_hang:
    hlt
    jmp .disk_hang

# ---------------------------------------------------------
# Data
# ---------------------------------------------------------

msg_loading: .asciz "Loading Kernel...\r\n"
msg_error:   .asciz "Disk Error!\r\n"
boot_drive:  .byte 0
kernel_exec_addr: .long 0x10000

# ---------------------------------------------------------
# GDT
# ---------------------------------------------------------
.align 8
gdt_start:
    .long 0x0, 0x0          # Null descriptor
gdt_code:
    .word 0xFFFF            # Limit (bits 0-15)
    .word 0x0000            # Base (bits 0-15)
    .byte 0x00              # Base (bits 16-23)
    .byte 0x9A              # Access byte (code)
    .byte 0xCF              # Flags + Limit (bits 16-19)
    .byte 0x00              # Base (bits 24-31)
gdt_data:
    .word 0xFFFF
    .word 0x0000
    .byte 0x00
    .byte 0x92              # Access byte (data)
    .byte 0xCF
    .byte 0x00
gdt_end:

gdt_descriptor:
    .word gdt_end - gdt_start - 1
    .long gdt_start

# The signature 0xAA55 is now added by the bootsector.ld linker script
