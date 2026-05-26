#ifndef _NOODLES_INTERNAL_H_
#define _NOODLES_INTERNAL_H_

#include <signal.h>
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
    void * cxt;
} ncontext_t;


typedef struct _schd_q {

    nthread_t t;
    ncontext_t cxt;
    struct _schd_q * next;
    struct _schd_q * prev;

} schd_q ;

typedef void (* sig_handler) (int) ;

#endif /** _NOODLES_INTERNALS_H_ */