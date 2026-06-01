#include "_noodles.h"
#include "noodles.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <setjmp.h>

static int active_scheduler = 0;
static int active_timer     = 0;
static int main_ctx_set     = 0;
static int tid_cnt          = 0;

schd_q * s_queue_cur  = NULL;
schd_q * s_queue_head = NULL;

ucontext_t prempt_ctx;

static sigjmp_buf jmp;
static volatile sig_atomic_t can_jmp = 0;

// sig_handler old_alarm_handler = NULL;


sig_handler alarm_handler (int sig, siginfo_t * info, ucontext_t * context) {
    // figure out how to go to scheduler without breaking reentrancy rules
    // longjmp to scheduler
    prempt_ctx = *context;
    siglongjmp (jmp, NULL);
    return NULL;
}

int disable_timer () {

    return 0;
}

int activate_timer () {

    // set timer interval
    struct itimerval timer;

    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = 150000;

    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = 100000;

    // register new handler
    // old_alarm_handler = signal(SIGALRM, new_alarm_handler);

    // start timer
    // setitimer(ITIMER_REAL, &timer, NULL);
    return 0;
}



int thread_schedule () {

    // setjmp here
    sigsetjmp (jmp, NULL);

    /* 1. Disable the timer, if active (so that scheduling and context switch 
          can happen without preemption) */

    if (active_timer) {
        disable_timer ();
        active_timer = 0;
    }

    // 2. Save current thread's context -> point at which timer prempted it
    // s_queue_cur->cxt = prempt_ctx;
    // 2. If thread count is one, i.e. only main thread remaining, exit from the loop. continue otherwise.
    // 3. Find the next thread to run.
    // 4. If next thread is different than current thread, load context, 

    register int y = 200;
    printf ("context will now be switched...\n");

    // save_ctx (&glb_ctx);
    // if (switch1) {
    //     switch1 = 0;
        // load_ctx (&s_queue_head->cxt);
    // }


    
    printf ("context was switched this won't be printed...\n");

    printf ("Is y changed after switch?: %d\n", y);
    if (!active_timer) {
        activate_timer();
    }


    return 0;
}

int noodles_create (nthread_t * nthread, void * (* nt_func) (void *), void * narg) {

    /* 1. Main: Initialize the scheduler queue if not done (lazy init), add main-worker */
    if (s_queue_head == NULL) {

        nthread_t t_main;
        t_main.tid = tid_cnt++;
        t_main.t_state = RUNNING;
        t_main.t_func = NULL;
        sigemptyset (&t_main.t_sig_mask);

        s_queue_cur = s_queue_head = malloc (sizeof (schd_q));
        s_queue_head->t = t_main;
        s_queue_head->next = s_queue_head;
        s_queue_head->prev = s_queue_head;
    }

    /* 2. Main: Set new thread's state and stack */
    nthread = malloc(sizeof(nthread_t));
    nthread->tid = tid_cnt++;
    nthread->t_state = READY;
    nthread->t_func = nt_func;
    sigemptyset (&nthread->t_sig_mask);

    nthread->t_stack = mmap (NULL, 2 * MAX_STACK_SIZE, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0); 

    if (nthread->t_stack == MAP_FAILED) {
        printf ("error: thread creation failed: stack space not available\n");
        return -1;
    }
    if (mprotect (nthread->t_stack, MAX_STACK_SIZE, PROT_NONE) == -1) {
        printf ("error: thread creation failed: stack guard not set\n");
        return -1;
    }
    nthread->t_stack = nthread->t_stack + 2 * MAX_STACK_SIZE - 1;

    
    /* 3. Main: Add thread to schedular queue (FIFO circular queue, hence new 
          thread is always at end i.e. prev of head) */

    schd_q * s_queue_ent = malloc (sizeof (schd_q));
    s_queue_ent->t = * nthread;
    // TODO: add context for current thread -> equal to nt_func add
    // s_queue_ent->cxt.sp = nthread->t_stack;
    s_queue_ent->next = s_queue_head;
    s_queue_ent->prev = s_queue_head->prev;
    s_queue_head->prev->next = s_queue_head->prev = s_queue_ent;

    register int x = 100;
    printf ("main thread created, now setting ctx...\n");
    goto main_worker_ctx;

start_schd:
    /* 4. Main: If scheduler is not running, start running it */
    if (!active_scheduler) {
        active_scheduler = 1;
        printf ("schedular started...\n");
        thread_schedule();
    }
main_worker_ctx: 
    if (!main_ctx_set) {
        main_ctx_set = 1;
        printf ("main context needs to be set...\n");
        // TODO: Main - worker: save context for the worker here
        // save_ctx (&s_queue_head->cxt);
    }
    if (!active_scheduler) {
        printf ("schedular needs to be started...\n");
        goto start_schd;
    }
    printf ("Is x changed after switch?:%d\n", x);
    return 0;
}

int noodles_join (nthread_t * nthread) {

    // wait for the thread to exit

    return 0;
}

int noodles_exit (nthread_t * nthread) {

    // remove the thread from the scheduling queue 
    // relink queue pointers 
    // decrease thread cnt
    // if the thread cnt reaches 1, i.e only main prog left, disable the timer. 
    // deallocated the thread from heap

    return 0;
}

int noodles_yield (nthread_t * nthread) {

    // put the thread on sleep/blocked queue
    return 0;
}
