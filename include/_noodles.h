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

#define MAX_STACK_SIZE      _get_pagesize()


typedef struct _nthread_t {

    int tid;
    int t_state;
    void * t_stack; 
    void * (* t_func) (void *);
    sigset_t t_sig_mask;

} nthread_t ;


typedef struct _ncontext_t {
    uint64_t x19, x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
    uint64_t x8;
    uint64_t lr;
    uint64_t sp;
} ncontext_t;


typedef struct _schd_q {

    nthread_t t;
    ncontext_t cxt;
    struct _schd_q * next;
    struct _schd_q * prev;

} schd_q ;

typedef void (* sig_handler) (int) ;

extern void save_ctx (void * ctx);
extern void load_ctx (void * ctx);
extern void switch_ctx (void * ctx);

#endif /** _NOODLES_INTERNALS_H_ */