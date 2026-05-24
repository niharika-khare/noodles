#include "_noodles.h"
#include "noodles.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/time.h>


static int active_scheduler = 0;
static int active_timer     = 0;
static int tid_cnt          = 1; // tid = 0 is the main prog

schd_q * s_queue_cur  = NULL;
schd_q * s_queue_head = NULL;

sig_handler old_alarm_handler = NULL;

sig_handler new_alarm_handler (int sig) {
    // figure out how to go to scheduler without breaking reentrancy rules
    // setjmp?
    return NULL;
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

int thread_switch() {

    return 0;

}


int thread_schedule () {

    // 1. Disable the timer, if active (so that scheduling and context switch 
    //    can happen without preemption)
    // 2. Find the next thread to run
    // 3. 



    if (!active_timer) {
        activate_timer();
    }


    return 0;
}

int noodles_create (nthread_t * nthread, void * (* nt_func) (void *), void * narg) {

    nthread = malloc(sizeof(nthread_t));
    nthread->tid = tid_cnt++;
    nthread->t_state = READY;
    nthread->t_func = nt_func;
    sigemptyset (&nthread->t_sig_mask);

    nthread->t_stack = mmap (NULL, MAX_STACK_SIZE, PROT_READ | PROT_WRITE | PROT_EXEC,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (nthread->t_stack == MAP_FAILED) {
        printf ("error: thread creation failed, stack space not available\n");
        return -1;
    }

    if (!active_scheduler) {
        // initialize the scheduler queue if not done (lazy init)
        nthread_t t_main;
        t_main.tid = 0;
        t_main.t_state = RUNNING;
        t_main.t_func = NULL;
        sigemptyset (&t_main.t_sig_mask);

        s_queue_cur = s_queue_head = malloc (sizeof (schd_q));
        s_queue_head->t = t_main;
        // add context switch for main
        s_queue_head->next = s_queue_head;
        s_queue_head->prev = s_queue_head;
    }

    // add to schedular queue
    schd_q * s_queue_ent = malloc (sizeof (schd_q));
    s_queue_ent->t = * nthread;
    s_queue_ent->next = s_queue_head;
    s_queue_ent->prev = s_queue_head->prev;
    s_queue_head->prev->next = s_queue_head->prev = s_queue_ent;
    

    // thread_schedule(); ??
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

    
    return 0;
}


