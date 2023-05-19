#include "chloros.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include <stdio.h>

// The run queue is a doubly linked list of threads. The front and the back of
// the list are tracked here.
static struct thread* runq_front;
static struct thread* runq_back;

// Push a thread onto the front of the run queue.
static void runq_push_front(struct thread* n) {
    n->next = runq_front;
    printf("runq_push_front: n->next\n");
    n->prev = NULL;
    printf("runq_push_front: n->prev\n");
    if (runq_front)
        runq_front->prev = n;
    else
        runq_back = n;
    runq_front = n;
}

// Push a thread onto the back of the run queue.
static void runq_push_back(struct thread* n) {
    n->next = NULL;
    n->prev = runq_back;
    if (runq_back)
        runq_back->next = n;
    else
        runq_front = n;
    runq_back = n;
}

// Remove a thread from the run queue.
static void runq_remove(struct thread* n) {
    if (n->next)
        n->next->prev = n->prev;
    else
        runq_back = n->prev;
    if (n->prev)
        n->prev->next = n->next;
    else
        runq_front = n->next;
}

static uint64_t next_id;

// Use a 2MB stack.
#define STACK_SIZE (1 << 21)

// Corresponds to thread_start in swtch.S.
void thread_start(void);

// Corresponds to ctxswitch in swtch.S.
void ctxswitch(struct context* old, struct context* new);

// The scheduler thread.
static struct thread* scheduler;

// The currently executing thread (or NULL).
static struct thread* running;

// This is the function that the scheduler runs. It should continuously pop a
// new thread from the run queue and attempt to run it.
static void schedule(void* _) {
    // (void) _;

    // FIXME: while there are still threads in the run queue, pop the next one
    // and run it. Make sure to adjust 'running' before you switch to it. Once
    // it context switches back to the scheduler by calling yield, you can
    // either put it back in the run queue (if it is still runnable), or
    // destroy it (if it exited).
    //
    // Hint: you can implement this how you like, but you might want to think
    // about what happens when the scheduler first starts up. At that point
    // 'running' might be the initial thread, which should first be put back on
    // the run queue before the main scheduler loop begins.
    
    printf("schedule: start scheduler\n");
    // if (scheduler->state == STATE_RUNNABLE) {
    //     runq_push_back(running);
    // }

    if (running->stack == NULL) {
        // the initial thread
        runq_push_back(running);
    }

    while (runq_front != NULL) {
        struct thread* next_thread = runq_front;
        runq_remove(next_thread);
        printf("schedule: running->id = %lu\n", running->id);
        printf("schedule: next_thread->id = %lu\n", next_thread->id);

        struct thread* previous_thread = running;
        running = next_thread;



        ctxswitch(&previous_thread->ctx, &next_thread->ctx);
        printf("ctxswitch in schedult: next_thread\n");

        if (previous_thread->state == STATE_EXITED) {
            free(previous_thread->stack);
            free(previous_thread);
        } else {
            runq_push_back(previous_thread);
        }
    }
}

// Creates a new thread that will execute fn(arg) when scheduled. Return the
// allocated thread, or NULL on failure.
static struct thread* thread_new(threadfn_t fn, void* arg) {
    // FIXME: allocate a new thread. This should give the thread a stack and
    // set up its context so that when you context switch to it, it will start
    // executing fn(arg).
    //
    // Hint: set up the thread so that when it gets context switched for the
    // first time, it will execute thread_start() with 'fn' at the top of the
    // stack, and arg above 'fn'.

    struct thread* t = (struct thread*) malloc(sizeof(struct thread));
    memset(t, 0, sizeof(struct thread));

    t->id = next_id++;
    t->state = STATE_RUNNABLE;
    t->ctx.mxcsr = 0x1F80;
    t->ctx.x87 = 0x037F;
    t->stack = (uint8_t*) malloc(STACK_SIZE);

    uint64_t stack_top = (uint64_t)(t->stack + STACK_SIZE);
    // uint64_t arg_addr = stack_top - sizeof(void*);
    // uint64_t fn_addr = arg_addr - sizeof(void*);
    // *((void**) arg_addr) = arg;
    // *((threadfn_t*) fn_addr) = fn;
    // t->ctx.rsp = fn_addr;
    uint64_t fn_addr = stack_top - sizeof(threadfn_t*);
    uint64_t arg_addr = fn_addr - sizeof(void**);
    *(void**) arg_addr = arg;
    *(threadfn_t*) fn_addr = fn;
    t->ctx.rsp = fn_addr;
    

    return t;


    // You'll want to initialize the context's mxcsr and x87 registers to the
    // following values:
    // t->ctx.mxcsr = 0x1F80;
    // t->ctx.x87 = 0x037F;
}

// Initializes the threading library. This should create the scheduler (a
// thread that executes scheduler(NULL)), and register the caller as a thread.
// Returns true on success and false on failure.
bool thread_init(void) {
    // FIXME: create the scheduler by allocating a new thread for it. You'll
    // want to store the newly allocated scheduler in 'scheduler'.
    
    scheduler = thread_new(schedule, NULL);
    printf("thread_init: thread_new after scheduler\n");

    // FIXME: register the initial thread (the currently executing context) as
    // a thread. It just needs a thread object but doesn't need a stack or any
    // initial context as it is already running. Make sure to update 'running'
    // since it is already running.

    // the currently executing context
    struct thread* t = (struct thread*) malloc(sizeof(struct thread));
    memset(t, 0, sizeof(struct thread));

    t->id = next_id++;
    t->state = STATE_RUNNABLE;
    t->ctx.mxcsr = 0x1F80;
    t->ctx.x87 = 0x037F;

    // running = scheduler;
    
    running = t;
    
    return true;
}

// Spawn a new thread. This should create a new thread for executing fn(arg),
// and switch to it immediately (push it on the front of the run queue, and
// switch to the scheduler).
bool thread_spawn(threadfn_t fn, void* arg) {
    // FIXME
    struct thread* t = thread_new(fn, arg);
    printf("thread_spawn: thread_new\n");
    runq_push_front(t);
    printf("thread_spawn: runq_push_front\n");
    ctxswitch(&running->ctx, &scheduler->ctx);
    printf("thread_spawn: ctxswitch\n");
    return true;
}

// Wait until there are no more other threads.
void thread_wait(void) {
    while (thread_yield()) {}
}

// Yield the currently executing thread. Returns true if it yielded to some
// other thread, and false if there were no other threads to yield to. If
// there are other threads, this should yield to the scheduler.
bool thread_yield(void) {
    assert(running != NULL);
    printf("thread_yield: assert running\n");
    // FIXME: if there are no threads in the run queue, return false. Otherwise
    // switch to the scheduler so that we can run one of them.
    if (runq_front == NULL) {
        return false;
    }
    printf("thread_yield: assert runq_front\n");
    printf("thread_yield: runq_front->id = %lu\n", runq_front->id);
    printf("thread_yield: running->id = %lu\n", running->id);
    printf("thread_yield: scheduler->id = %lu\n", scheduler->id);
    // struct thread* yield_to_thread = runq_front;
    // runq_remove(yield_to_thread);
    
    ctxswitch(&running->ctx, &scheduler->ctx);

    // printf("thread_yield: switch to scheduler\n");

    // struct thread* executing_thread = running;
    // running = yield_to_thread;

    // ctxswitch(&executing_thread->ctx, &yield_to_thread->ctx);
    // printf("thread_yield: assert finish ctx switch\n");

    // if (executing_thread->state == STATE_EXITED) {
    //     free(executing_thread->stack);
    //     free(executing_thread);
    // } else {
    //     runq_push_back(executing_thread);
    // }
    printf("thread_yield: return from scheduler ctxswitch\n");
    return true;
}

// The entrypoint for a new thread. This should call the requested function
// with its argument, and then do any necessary cleanup after the function
// completes (to cause the thread to exit).
void thread_entry(threadfn_t fn, void* arg) {
    fn(arg);
    printf("thread_entry: fn(arg)\n");
    running->state = STATE_EXITED;
    printf("thread_entry: running state\n");
    thread_yield();
    printf("thread_entry: thread_yield\n");
    // this should never happen
    assert(!"exited thread resumed");
}
