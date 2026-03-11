# -----------------------------
# Kernel Loader (Flat Binary)
# -----------------------------
.set MULTIBOOT_MAGIC,  0x1BADB002
.set MULTIBOOT_FLAGS,  0x00000003
.set MULTIBOOT_CHECKSUM, -(MULTIBOOT_MAGIC + MULTIBOOT_FLAGS)

.section .multiboot
.align 4
    .long MULTIBOOT_MAGIC
    .long MULTIBOOT_FLAGS
    .long MULTIBOOT_CHECKSUM

.section .text
.global _start

# -----------------------------
# Stack (16 KB aligned)
# -----------------------------
.section .bss
.align 16
stackBottom:
    .skip 16384      # 16 KB stack
stackTop:

# -----------------------------
# Text section
# -----------------------------
.section .text
.global _start
# _start: entry point from GRUB
_start:
    # Disable interrupts
    cli

    # Set up stack
    mov $stackTop, %esp

    # GRUB passes multiboot info pointer in EBX
    # Push it to stack for kmain(multiboot_info_t* mbd)
    push %ebx
    
    # Zero out .bss section
    .extern _sbss
    .extern _ebss
    mov $_sbss, %edi
    mov $_ebss, %ecx
    sub %edi, %ecx
    xor %eax, %eax
    rep stosb

    # Call the kernel entry point
    call kmain

    # Clean up stack (not strictly needed since we halt)
    add $4, %esp

halt_loop:
    hlt
    jmp halt_loop
