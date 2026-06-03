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

static schd_q * s_queue_cur  = NULL;
static schd_q * s_queue_head = NULL;

static mcontext_t prempt_ctx;

static sigjmp_buf jmp;
static volatile sig_atomic_t can_jmp = 0;

static sig_handler old_alarm_handler = NULL;


static void alarm_handler (int sig, siginfo_t * info, void * context) {

    char msg[30] = "Inside the signal handler...\n";
    write (STDOUT_FILENO, msg, sizeof (msg));
    ucontext_t * u_ctx = (ucontext_t *) context;
    prempt_ctx = u_ctx->uc_mcontext;
    
    siglongjmp (jmp, 2);

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
    t_spec.it_value.tv_nsec = 100000;  
    t_spec.it_interval.tv_sec = 0;  
    t_spec.it_interval.tv_nsec = 100000; 

    if (timer_settime (switch_timer, 0, &t_spec, NULL) == -1) {
        printf ("err: unable to start switch timer\n");
        return -1;
    }
    return 0;
}



static int thread_schedule () {

    if (init_timer() == -1) {
        printf ("err: unable to create switch timer!\n");
        return -1;
    }

    if (sigsetjmp (jmp, 2) != 0) {
        printf ("Here after the context switch jmp\n");
    }
    

    /* Disable preemption while scheduling and context switch */
    if (active_timer) {
        disable_timer ();
        active_timer = 0;

        schd_q * prev_thread = s_queue_cur;
        s_queue_cur = s_queue_cur->next;

        prev_thread->cxt = prempt_ctx;
        printf ("Saved context for tid: %d\n", prev_thread->t.tid);
        if (prev_thread->t.t_state == FINISHED) {
            printf ("Deallocating prev finished thread %d...\n", prev_thread->t.tid);
            free (prev_thread);
        }
    }    

    if (s_queue_cur->next == s_queue_cur && s_queue_cur == s_queue_head) {

        printf ("No thread in queue, resuming main-worker context...\n");

        active_scheduler = 0;
        main_ctx_set     = 0;
        tid_cnt          = 0;
        active_timer     = 0;

        mcontext_t main_ctx = s_queue_cur->cxt;
        s_queue_head = s_queue_cur = NULL;

        load_ctx (&main_ctx);
    } 
    else {
        /* main worker thread */
        if (s_queue_cur == s_queue_head) {
            printf ("Picked up main thread...\n");
        }
        else if (s_queue_cur->t.t_state == READY) {
            printf ("Picked up user thread %d...\n", s_queue_cur->t.tid);

            s_queue_cur->prev->next = s_queue_cur->next;
            s_queue_cur->next->prev = s_queue_cur->prev;

            sleep (1);
            s_queue_cur->t.t_state = FINISHED;

            printf ("Finished user thread now sleeping...\n");

            // begin thread function execution
            // assign the declared the stack for the thread here?
            // initialize thread context here?

            // 4. If next thread is different than current thread, load context, 
        }

        if (!active_timer) {
            active_timer = 1;
            activate_timer();
        }
        if (s_queue_cur != s_queue_head) sleep (1);

        printf ("Loading context of tid %d...\n", s_queue_cur->t.tid);
        load_ctx (&s_queue_cur->cxt);
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
    if (s_queue_head == NULL) {
        printf ("Initializing main thread...\n");

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
    printf ("Initializing requested thread...\n");
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
    s_queue_ent->next = s_queue_head;
    s_queue_ent->prev = s_queue_head->prev;
    s_queue_head->prev->next = s_queue_ent;
    s_queue_head->prev = s_queue_ent;

    goto main_worker_ctx;

start_schd:
    /* 4. Main: If scheduler is not running, start running it */
    if (!active_scheduler) {
        active_scheduler = 1;
        printf ("Starting thread schedular context...\n");
        return thread_schedule();
    }
main_worker_ctx: 
    if (!main_ctx_set) {
        main_ctx_set = 1;
        printf ("Setting main thread's context...\n");
        save_ctx (&s_queue_head->cxt);
    }
    if (!active_scheduler && s_queue_head) {
        goto start_schd;
    }

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
