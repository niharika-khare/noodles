#include "_noodles.h"
#include "noodles.h"

#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>

static int active_scheduler = 0;
static int active_timer     = 0;
static int main_ctx_set     = 0;
static int tid_cnt          = 0;

static nthread_t * tq_cur  = NULL;
static nthread_t * tq_head = NULL;

// static mcontext_t prempt_ctx;
// static sigjmp_buf jmp;


static int thread_schedule (mcontext_t ctx);

unsigned int sleep (unsigned int seconds) {

    struct timespec req;
    struct timespec rem;

    req.tv_sec = seconds;
    req.tv_nsec = 0;

    while (nanosleep (&req, &rem) == -1) {
        req.tv_sec = rem.tv_sec;
        req.tv_nsec = rem.tv_nsec;
    }
    return 0;
}


static void alarm_handler (int sig, siginfo_t * info, void * context) {

    char msg[30] = "Inside the signal handler...\n";
    write (STDOUT_FILENO, msg, sizeof (msg));
    ucontext_t * u_ctx = (ucontext_t *) context;
    
    thread_schedule (u_ctx->uc_mcontext);

    u_ctx->uc_mcontext = tq_cur->cxt;
    
    // siglongjmp (jmp, 2);

}

static timer_t switch_timer;


static int init_timer () {

    printf ("Creating context switch timer...\n");
    struct sigaction sa;
    sa.sa_sigaction = alarm_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART;
    sigaction (SIGUSR1, &sa, NULL);

    struct sigevent sigev;

    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo  = SIGUSR1;
    sigev.sigev_notify_attributes = NULL;

    return timer_create (CLOCK_MONOTONIC, &sigev, &switch_timer);
}

static int disable_timer () {

    printf ("Disabling timer...\n");

    struct itimerspec t_spec_pause = {0};

    if (timer_settime (switch_timer, 0, &t_spec_pause, NULL) == -1) {
        printf ("err: unable to pause switch timer\n");
        return -1;
    }
    return 0;
}

static int activate_timer () {

    printf ("Activating timer...\n");

    struct itimerspec t_spec;
    t_spec.it_value.tv_sec = 0;    
    t_spec.it_value.tv_nsec = 10000;  
    t_spec.it_interval.tv_sec = 0;  
    t_spec.it_interval.tv_nsec = 10000; 

    if (timer_settime (switch_timer, 0, &t_spec, NULL) == -1) {
        printf ("err: unable to start switch timer\n");
        return -1;
    }
    return 0;
}



static int thread_schedule (mcontext_t ctx) {

    // if (init_timer() == -1) {
    //     printf ("err: unable to create switch timer!\n");
    //     return -1;
    // }

    // if (sigsetjmp (jmp, 2) != 0) {
    //     printf ("Here after the context switch jmp\n");
    // }
    

    /* Disable preemption while scheduling and context switch */
    if (active_timer) {
        disable_timer ();
        active_timer = 0;

        nthread_t * prev_thread = tq_cur;
        tq_cur = tq_cur->next;

        prev_thread->cxt = ctx;
        printf ("Saved context for tid: %d\n", prev_thread->tid);
        if (prev_thread->t_state == FINISHED) {
            printf ("Deallocating prev finished thread %d...\n", prev_thread->tid);
            free (prev_thread);
        }
    }    

    if (tq_cur->next == tq_cur && tq_cur == tq_head) {

        printf ("No thread in queue, resuming main-worker context...\n");

        active_scheduler = 0;
        main_ctx_set     = 0;
        tid_cnt          = 0;
        active_timer     = 0;

        // mcontext_t main_ctx = tq_cur->cxt;
        // tq_head = tq_cur = NULL;

        // load_ctx (&main_ctx);
    } 
    else {
        /* main worker thread */
        if (tq_cur == tq_head) {
            printf ("Picked up main thread...\n");
            tq_cur->cxt = ctx;
            
        }
        else if (tq_cur->t_state == READY) {
            printf ("Picked up user thread %d...\n", tq_cur->tid);

            // change thread state
            tq_cur->t_state = RUNNING;
            
            tq_cur->t_stack = (char *) tq_cur->t_stack - sizeof (uintptr_t);
            // stack -> push the exit func and args, push thread func and args
            *((uintptr_t *) tq_cur->t_stack) = (uintptr_t) (tq_cur->t_func);
            
            printf ("Here after pushing function on stack\n");

            // enable timer
            if (!active_timer) {
                active_timer = 1;
                activate_timer();
            }
            // inline asm for executing function on the stack
            __asm__ __volatile__ (
                "mov    sp, %0;"
                "mov    x0, %1;"
                :
                : "r" (tq_cur->t_stack), "r" (tq_cur->t_arg)
            );


            // begin thread function execution
            // assign the declared the stack for the thread here?
            // initialize thread context here?

            // 4. If next thread is different than current thread, load context, 
        } 

        if (!active_timer) {
            active_timer = 1;
            activate_timer();
        }
        if (tq_cur != tq_head) sleep (1);

        // printf ("Loading context of tid %d...\n", tq_cur->t.tid);
        // load_ctx (&tq_cur->cxt);
    }

    return 0;
}

/**
 * Create a thread, triggered by user's request.
 * If there exists no active thread, then:
 *  1. The main-worker thread would be created.
 *  2. The scheduler would be started/restarted.
 */
int noodles_create (nthread_t * nthread, void * (* nt_func) (void *), void * narg) {

    /* 1. Main: Initialize the scheduler queue if not done (lazy init), add main-worker */
    if (tq_head == NULL) {
        printf ("Initializing main thread...\n");

        nthread_t * t_main = malloc (sizeof (nthread_t));
        t_main->tid = tid_cnt++;
        t_main->t_state = RUNNING;
        t_main->t_func = NULL;
        t_main->t_arg = NULL;
        sigemptyset (&t_main->t_sig_mask);
        t_main->next = t_main->prev = t_main;

        tq_cur = tq_head = t_main;
    }

    /* 2. Main: Set new thread's state and stack */
    printf ("Initializing requested thread...\n");
    nthread = malloc(sizeof(nthread_t));
    nthread->tid = tid_cnt++;
    nthread->t_state = READY;
    nthread->t_func = nt_func;
    nthread->t_arg = narg;
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

    nthread->t_stack = nthread->t_stack + 2 * MAX_STACK_SIZE;
    // *((uintptr_t *) nthread->t_stack) = (uintptr_t) nthread->t_func;
    // nthread->cxt.sp = (long long unsigned int) nthread->t_stack;

    /* 3. Main: Add thread to schedular queue (FIFO circular queue, hence new 
          thread is always at end i.e. prev of head) */
    nthread->next = tq_head;
    nthread->prev = tq_head->prev;
    tq_head->prev->next = nthread;
    tq_head->prev = nthread;
    
    goto main_worker_ctx;

start_schd:
    /* 4. Main: If scheduler is not running, start running it */
    // if (!active_scheduler) {
    //     active_scheduler = 1;
    //     printf ("Starting thread schedular context...\n");
    //     return thread_schedule();
    // }
    if (!active_scheduler) {
        active_scheduler = 1;
        if (init_timer() == -1) {
            printf ("err: unable to create switch timer!\n");
            return -1;
        }
        active_timer = 1;
        activate_timer ();
    }
main_worker_ctx: 
    if (!main_ctx_set) {
        main_ctx_set = 1;
        printf ("Setting main thread's context...\n");
        save_ctx (&tq_head->cxt);
    }
    if (!active_scheduler && tq_head) {
        goto start_schd;
    }

    return 0;
}

int noodles_join (nthread_t * nthread) {

    // wait for the thread to exit

    return 0;
}

/**
 * Terminate the current thread.
 * Schedular requests this to remove thread from the queue.
 */
int noodles_exit (nthread_t * nthread) {

    if (tq_cur == nthread) {
        tq_cur->prev->next = tq_cur->next;
        tq_cur->next->prev = tq_cur->prev;

        tq_cur->t_state = FINISHED;

        return 0;
    }

    return -1;
}

int noodles_yield (nthread_t * nthread) {

    // put the thread on sleep/blocked queue
    return 0;
}
