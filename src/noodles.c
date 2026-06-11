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
static int tid_cnt          = 0;

static timer_t switch_timer;

static nthread_t * tq_cur  = NULL;
static nthread_t * tq_head = NULL;

static int init_timer ();
static int disable_timer ();
static int activate_timer ();
static int thread_schedule (mcontext_t);
static void custom_alarm_signal_handler (int, siginfo_t *, void *);


static inline pid_t get_pid () {
    static pid_t pid = -1;
    if (pid == -1) {
        return getpid ();
    }
    return pid;
}


/** Initialize the timer only once */
static int init_timer () {

    /* Defining signal for switch timer */
    stack_t sigstack;
    sigstack.ss_sp = malloc (SIGSTKSZ);
    sigstack.ss_size = SIGSTKSZ;
    sigstack.ss_flags = 0;
    sigaltstack (&sigstack, NULL);

    struct sigaction sa;
    sa.sa_sigaction = custom_alarm_signal_handler;
    sigemptyset (&sa.sa_mask);
    sa.sa_flags = SA_SIGINFO | SA_RESTART | SA_ONSTACK;
    sigaction (SIGUSR1, &sa, NULL);

    /* Creating context switch timer */
    struct sigevent sigev;

    sigev.sigev_notify = SIGEV_SIGNAL;
    sigev.sigev_signo  = SIGUSR1;
    sigev.sigev_notify_attributes = NULL;

    return timer_create (CLOCK_MONOTONIC, &sigev, &switch_timer);
}

/** 
 * Inside the signal handler->thread_schdule, timer is disabled. It is 
 * restarted just before the context switch. This is to ensure that timer 
 * interrupts are uniform for all threads.
 * */
static int disable_timer () {

    struct itimerspec t_spec_pause = {0};

    if (timer_settime (switch_timer, 0, &t_spec_pause, NULL) == -1) {
        printf ("err: unable to pause switch timer\n");
        return -1;
    }
    return 0;
}

static int activate_timer () {

    struct itimerspec t_spec;
    t_spec.it_value.tv_sec = 0;    
    t_spec.it_value.tv_nsec = 1000000;  
    t_spec.it_interval.tv_sec = 0;  
    t_spec.it_interval.tv_nsec = 1000000; 

    if (timer_settime (switch_timer, 0, &t_spec, NULL) == -1) {
        printf ("err: unable to start switch timer\n");
        return -1;
    }
    return 0;
}


/**
 * The thread scheduler is called from the inside the signal handler and is 
 * responsible for disabling/enabling the timer, picking threads in RR, 
 * saving context of the pre-empted thread, state management of thread and
 * modification of relevant flags.
 */
static int thread_schedule (mcontext_t ctx) {

    /* Disable preemption while scheduling and context switch */
    if (active_timer) {
        disable_timer ();
        active_timer = 0;

        nthread_t * prev_thread = tq_cur;
        tq_cur = tq_cur->next;

        prev_thread->cxt = ctx;
        if (prev_thread->t_state == FINISHED) {
            munmap (prev_thread->t_stack, 2 * MAX_STACK_SIZE);
        }
    }    
    /* If main is the only thread then return withour re-enabling the timer */
    if (tq_cur->next == tq_cur && tq_cur == tq_head) {

        active_scheduler = 0;
        active_timer     = 0;
        /* tid=0 is main, main thread is not removed from the queue */
        tid_cnt          = 1;
    } 
    else {
        if (tq_cur->t_state == RUNNABLE) {

            tq_cur->t_state = RUNNING;
            /* 
             On aarch64, 16 bytes alignment is mandatory, therefore before writing to 
             stack, the stack pointer needs to be moved downwards otherwise seg fault 
             will occur 
            */
            tq_cur->t_stack = (char *) tq_cur->t_stack - 16;

            /* 
             Signal handler blocks the source signal. Need to unblock the source signal 
             here as the thread would not be returning back to the signal handler 
            */
            sigset_t unblock;
            sigemptyset (&unblock);
            sigaddset (&unblock, SIGUSR1);
            sigprocmask (SIG_UNBLOCK, &unblock, NULL);

            /* Enable timer before context switch to enable further preemption */
            if (!active_timer) {
                active_timer = 1;
                activate_timer();
            }

            /* Set stack and executing function on the stack */
            __asm__ __volatile__ (
                "mov    sp,  %0;"
                "mov    x0,  %1;"
                "mov    x1,  %2;"
                "mov    x30, %3;"
                "br     x1;"
                :
                : "r" (tq_cur->t_stack), "r" (tq_cur->t_arg), 
                  "r" (tq_cur->t_func), "r" (noodles_exit)
            );
        } 
        else if (!active_timer) {
            active_timer = 1;
            activate_timer();
        }
        
    }
    return 0;
}

/**
 * Handler for the real time signal responsible for thread preemption.
 * This handler is responsible for context switch.
 * By replacing the uc_mcontext of the interrupted thread, the kernel's
 * sigreturn will use the updated uc_mcontext and result into the 
 * switching of context with full set of GP, SP, PC, CPSR, FP/SMID registers.
 * 
 * An alternate apprach of switching context via hand rolled assembly was also 
 * attempted. While it worked well to replace the registers that are part of the
 * mcontext_t struct, SMID registers were lost. Need kernel's help in restoring 
 * those, hence need the sigreturn from inside the signal handler.
 * */
static void custom_alarm_signal_handler (int sig, siginfo_t * info, void * context) {

    ucontext_t * u_ctx = (ucontext_t *) context;
    thread_schedule (u_ctx->uc_mcontext);
    u_ctx->uc_mcontext = tq_cur->cxt;

}

/**
 * Create a thread, triggered by user's request.
 * If there exists no active thread, then:
 *  1. The main-worker thread would be created.
 *  2. The scheduler would be started via a custom timer.
 */
int noodles_create (nthread_t * nthread, void * (* nt_func) (void *), void * narg) {

    /* Initialize the scheduler queue if not done (lazy init), add main-worker */
    if (tq_head == NULL) {

        nthread_t * t_main = malloc (sizeof (nthread_t));
        t_main->tid = tid_cnt++;
        t_main->t_state = RUNNING;
        t_main->t_func = NULL;
        t_main->t_arg = NULL;
        sigemptyset (&t_main->t_sig_mask);
        t_main->next = t_main->prev = t_main;

        tq_cur = tq_head = t_main;
        
        if (init_timer() == -1) {
            printf ("err: unable to create switch timer!\n");
            return -1;
        }
    }

    /* Set new thread's state and stack */
    nthread->tid = tid_cnt++;
    nthread->t_state = RUNNABLE;
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

    /* 
     Add thread to scheduler queue (FIFO circular queue, hence new thread 
     is always at end i.e. prev of head)
    */
    nthread->next = tq_head;
    nthread->prev = tq_head->prev;
    tq_head->prev->next = nthread;
    tq_head->prev = nthread;
    
    /* 
     Activated whenever the user requests at least one active thread. 
     If main is the only thread preemption is not needed 
    */
    if (!active_scheduler) {
        active_scheduler = 1;
        active_timer = 1;
        activate_timer ();
    }

    return 0;
}

/**
 * Terminate the current thread.
 * Schedular also calls this to remove thread from the queue.
 */
int noodles_exit () {

    /* 
     Disable preemption for atomicity, will get re-enabled in 
     scheduler after kill () 
    */
    disable_timer ();

    tid_cnt--;
    tq_cur->t_state = FINISHED;

    tq_cur->prev->next = tq_cur->next;
    tq_cur->next->prev = tq_cur->prev;
    
    /* 
     Send signal to self to invoke the thread scheduler for immediately 
     removing the finished thread and pick up the next thread
    */
    kill (get_pid (), SIGUSR1);

    return 0;
}

/**
 * Wait for the thread to finish execution, yield is thread is not finished.
 */
int noodles_join (nthread_t * nthread) {

    while (nthread->t_state != FINISHED)
    {
        kill (get_pid (), SIGUSR1);
    }
    return 0;
}

int noodles_yield () {

    /* 
     Disable preemption for atomicity, will get re-enabled in 
     scheduler after kill () 
    */
    disable_timer ();

    kill (get_pid (), SIGUSR1);

    return 0;
}
