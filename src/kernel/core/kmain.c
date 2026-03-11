#include "kernel/multiboot.h"
#include "OS/OS.h"
void kmain(multiboot_info_t* mbd) {
    main_os(mbd);
}
