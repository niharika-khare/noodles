#ifndef _NOODLES_INTERNAL_H_
#define _NOODLES_INTERNAL_H_

#include <signal.h>
#include <stdint.h>
#include <unistd.h>

static inline size_t _get_pagesize () {

    static size_t _page_size = 0;
    if (_page_size == 0) {
        _page_size = sysconf(_SC_PAGESIZE);
    }
    return _page_size;
}


/* Thread states */

#define READY               1
#define RUNNING             2
#define BLOCKED             4
#define FINISHED            8

#define MAX_STACK_SIZE      _get_pagesize()


typedef struct _nthread_t {

    int tid;
    int t_state;
    void * t_stack; 
    void * (* t_func) (void *);
    void * t_arg;
    sigset_t t_sig_mask;
    mcontext_t cxt;
    struct _nthread_t * next;
    struct _nthread_t * prev;

} nthread_t ;

// typedef struct _schd_q {

//     nthread_t t;
//     mcontext_t cxt;
//     struct _schd_q * next;
//     struct _schd_q * prev;

// } schd_q ;

typedef void (* sig_handler) (int) ;

extern void save_ctx (void * ctx);
extern void load_ctx (void * ctx);
extern void switch_ctx (void * ctx);

#endif /** _NOODLES_INTERNALS_H_ */