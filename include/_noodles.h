#ifndef _NOODLES_INTERNAL_H_
#define _NOODLES_INTERNAL_H_

#include <signal.h>


#define READY       1
#define RUNNING     2
#define BLOCKED     4


#define MAX_STACK_SIZE  16 * 1024


typedef struct _nthread_t {
    int tid;
    int t_state;
    sigset_t t_sig_mask;
    void * t_stack; 
    void * (* t_func) (void *);
} nthread_t;


typedef struct _ncontext_t {

} ncontext_t;


typedef struct _schd_q {
    nthread_t t;
    ncontext_t cxt;
    struct _schd_q * next;
    struct _schd_q * prev;
} schd_q;

typedef void (*sig_handler)(int);

#endif /** _NOODLES_INTERNALS_H_ */