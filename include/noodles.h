#ifndef _NOODLES_H_
#define _NOODLES_H_

#include "_noodles.h"
#include "common.h"

int noodles_create (nthread_t * nthread, void * (* t_func) (void *), void * tf_arg);
int noodles_join (nthread_t * nthread);
int noodles_yield (nthread_t * nthread);
int noodles_exit ();

#endif /** _NOODLES_H_ */