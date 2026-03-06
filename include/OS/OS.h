//
// Created by s28 on 3/6/2026.
//

#ifndef SBOY28_OS_H
#define SBOY28_OS_H

#include <stdint.h>

#define OS_MAX_APPS 4

typedef enum {
    OS_APP_STOPPED = 0,
    OS_APP_RUNNING,
    OS_APP_SUSPENDED,
    OS_APP_TERMINATED
} os_app_state_t;

void os_shutdown(void);
void os_restart(void);

int main(void);

#endif //SBOY28_OS_H
