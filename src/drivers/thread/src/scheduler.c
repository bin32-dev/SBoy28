/*
 * Minimal preemptive-ready scheduler.
 *
 * Notes:
 * - Assumes 32-bit protected mode, GCC (AT&T) assembly context_switch.
 * - Replace kmalloc/kfree and irq mask/unmask with your kernel's helpers.
 * - This scheduler uses a single run-queue (FIFO round-robin).
 */

#include "drivers/thread.h"
#include "kernel/kheap.h"
#include "kernel/spinlock.h"
#include "kernel/cpu.h"
#include <stdint.h>
#include <stddef.h>

/* context_switch(prev, next) implemented in context_switch.S */
extern void context_switch(thread_t *prev, thread_t *next);

/* thread trampoline executed when new thread first runs */
static void thread_trampoline(void (*fn)(void*), void *arg);

/* Runqueue head/tail */
static thread_t *run_head = NULL;
static thread_t *run_tail = NULL;

/* Current running thread */
static thread_t *current = NULL;
static thread_t *deferred_reap = NULL;

/* Scheduler lock */
static spinlock_t sched_lock;

/* Simple id generator */
static tid_t next_tid = 1;

/* Default stack size if not provided */
#ifndef THREAD_DEFAULT_STACK
#define THREAD_DEFAULT_STACK (8*1024)
#endif

/* Enqueue thread to runqueue (internal, assumes lock held) */
static void enqueue_thread_locked(thread_t *t)
{
    t->next = NULL;
    if (!run_head) {
        run_head = run_tail = t;
    } else {
        run_tail->next = t;
        run_tail = t;
    }
}

/* Public enqueue */
static void enqueue_thread(thread_t *t)
{
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);
    enqueue_thread_locked(t);
    spinlock_unlock(&sched_lock);
    restore_irq_state(flags);
}

/* Dequeue next runnable (internal, assumes lock held) */
static thread_t *dequeue_thread_locked(void)
{
    thread_t *t = run_head;
    if (!t) return NULL;
    run_head = t->next;
    if (!run_head) run_tail = NULL;
    t->next = NULL;
    return t;
}



static uint32_t *prepare_stack(uint32_t *stack_base, size_t stack_size,
                               thread_fn_t fn, void *arg)
{
    /* stack grows down */
    uint32_t *sp = (uint32_t *)((uint8_t *)stack_base + stack_size);

    /* push args and dummy return address for thread_trampoline */
    sp -= 1;
    *sp = (uint32_t)arg;

    sp -= 1;
    *sp = (uint32_t)fn;

    sp -= 1;
    *sp = 0; /* dummy return address */

    /* push thread_trampoline as the EIP for 'ret' from context_switch */
    sp -= 1;
    *sp = (uint32_t)thread_trampoline;

    /* push 8 registers for popa */
    sp -= 8;
    for (int i = 0; i < 8; ++i) sp[i] = 0;

    return sp;
}

/* Create a thread */
int thread_system_init(void)
{
    run_head = run_tail = NULL;
    current = NULL;
    deferred_reap = NULL;
    next_tid = 1;
    spinlock_init(&sched_lock);
    return 0;
}

static void reap_deferred_thread(void)
{
    if (!deferred_reap) {
        return;
    }

    thread_t *zombie = deferred_reap;
    deferred_reap = NULL;

    if (zombie->stack) {
        kfree(zombie->stack);
        zombie->stack = NULL;
    }

    kfree(zombie);
}

tid_t thread_create(thread_fn_t fn, void *arg, size_t stack_size)
{
    if (!stack_size) stack_size = THREAD_DEFAULT_STACK;

    thread_t *t = (thread_t *)kmalloc(sizeof(thread_t));
    if (!t) return 0;

    t->stack = (uint32_t *)kmalloc(stack_size);
    if (!t->stack) {
        kfree(t);
        return 0;
    }
    t->stack_size = stack_size;
    t->id = next_tid++;
    t->state = THREAD_READY;
    t->next = NULL;

    /* prepare initial stack */
    uint32_t *esp = prepare_stack(t->stack, t->stack_size, fn, arg);
    t->esp = (uint32_t)esp;

    enqueue_thread(t);
    return t->id;
}

/* Switcher helper invoked by IRQ or yield path.
 * We pick the next runnable thread (round-robin).
 */
static void schedule(void)
{
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    thread_t *next = dequeue_thread_locked();
    if (!next) {
        /* nothing to run; keep running current if any */
        spinlock_unlock(&sched_lock);
        restore_irq_state(flags);
        return;
    }

    thread_t *prev = current;

    if (prev && prev->state == THREAD_RUNNING) {
        prev->state = THREAD_READY;
        enqueue_thread_locked(prev);
    }

    next->state = THREAD_RUNNING;
    current = next;

    /* Before context switch, we must release the lock and restore interrupts.
     * BUT: The lock protects the transition itself. In a single-core system,
     * releasing before switch is okay because interrupts are still disabled
     * by the 'flags' we saved.
     */
    spinlock_unlock(&sched_lock);
    
    if (prev == NULL) {
        /* first switch into a thread: call context_switch with prev==NULL */
        /* context_switch will eventually restore flags when the thread runs */
        context_switch(NULL, next);
    } else {
        context_switch(prev, next);
    }
    
    /* When we return here, we are back in the original thread.
     * We need to restore the original interrupt state.
     */
    restore_irq_state(flags);
}

/* public: yield control voluntarily */
void thread_yield(void)
{
    reap_deferred_thread();

    /* quick path: if only one thread, return */
    if (!run_head || run_head == current) return;

    schedule();
}

/* Called from IRQ context (timer) to perform preemption attempt */
void scheduler_tick_from_irq(void)
{
    /* For safety, keep this short. Decide to reschedule each tick. */
    /* In production, use quantum counters; here we preempt every tick. */
    if (!run_head) return;

    schedule();
}

/* thread_exit - terminate current thread (simple) */
void thread_exit(void)
{
    if (!current) return;

    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    current->state = THREAD_ZOMBIE;
    deferred_reap = current;
    current = NULL;

    spinlock_unlock(&sched_lock);
    restore_irq_state(flags);

    schedule();

    /* if schedule returns here (shouldn't), halt */
    for (;;);
}

/* The trampoline invoked when a new thread is started. Implementation detail:
 * When context_switch 'ret's into thread_trampoline, the stack is arranged so that
 * thread_trampoline's arguments (fn,arg) are accessible from the stack.
 */
static void thread_trampoline(void (*fn)(void*), void *arg)
{
    /* Enable interrupts so this new thread can be preempted */
    __asm__ volatile("sti");

    /* call the real function */
    fn(arg);

    /* when function returns, exit the thread */
    thread_exit();
}

/* ---------------- SYNCHRONIZATION ---------------- */

void mutex_init(mutex_t *mut) {
    if (!mut) return;
    mut->locked = 0;
    mut->wait_head = NULL;
    mut->wait_tail = NULL;
}

void mutex_lock(mutex_t *mut) {
    if (!mut) return;
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    if (mut->locked == 0) {
        mut->locked = 1;
        spinlock_unlock(&sched_lock);
        restore_irq_state(flags);
        return;
    }

    current->state = THREAD_SLEEPING;
    current->next = NULL;
    if (!mut->wait_head) {
        mut->wait_head = mut->wait_tail = current;
    } else {
        mut->wait_tail->next = current;
        mut->wait_tail = current;
    }

    spinlock_unlock(&sched_lock);
    schedule();

    restore_irq_state(flags);
}

void mutex_unlock(mutex_t *mut) {
    if (!mut) return;
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    if (mut->wait_head) {
        thread_t *t = mut->wait_head;
        mut->wait_head = t->next;
        if (!mut->wait_head) mut->wait_tail = NULL;
        
        t->state = THREAD_READY;
        enqueue_thread_locked(t);
    } else {
        mut->locked = 0;
    }

    spinlock_unlock(&sched_lock);
    restore_irq_state(flags);
}

void sem_init(semaphore_t *sem, int count) {
    if (!sem) return;
    sem->count = count;
    sem->wait_head = sem->wait_tail = NULL;
}

void sem_wait(semaphore_t *sem) {
    if (!sem) return;
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    if (sem->count > 0) {
        sem->count--;
        spinlock_unlock(&sched_lock);
        restore_irq_state(flags);
        return;
    }

    current->state = THREAD_SLEEPING;
    current->next = NULL;
    if (!sem->wait_head) {
        sem->wait_head = sem->wait_tail = current;
    } else {
        sem->wait_tail->next = current;
        sem->wait_tail = current;
    }

    spinlock_unlock(&sched_lock);
    schedule();

    restore_irq_state(flags);
}

void sem_post(semaphore_t *sem) {
    if (!sem) return;
    uint32_t flags = save_irq_state();
    spinlock_lock(&sched_lock);

    if (sem->wait_head) {
        thread_t *t = sem->wait_head;
        sem->wait_head = t->next;
        if (!sem->wait_head) sem->wait_tail = NULL;
        
        t->state = THREAD_READY;
        enqueue_thread_locked(t);
    } else {
        sem->count++;
    }

    spinlock_unlock(&sched_lock);
    restore_irq_state(flags);
}
