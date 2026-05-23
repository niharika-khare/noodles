#ifndef _NOODLES_INTERNAL_H_
#define _NOODLES_INTERNAL_H_



typedef struct _nthread_t {
    int tid;
    int t_state;
    int t_sig_mask;
    int *t_stack[1000]; // using static stack for now, mmap later
    void * (* t_func) (void *);
} nthread_t;


#endif /** _NOODLES_INTERNALS_H_ */