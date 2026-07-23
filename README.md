# Noodles

A user-level (N:1) thread library for AArch64 Linux, written in C.

Noodles implements its own thread type, scheduler, and preemption mechanism entirely in user space — no `pthread_create`, no `clone()`. All threads run as execution contexts multiplexed onto a single underlying process/kernel thread.

## Features

- **Thread lifecycle API** — create, yield, join, and exit for user-level threads (`nthread_t`)
- **Preemptive round-robin scheduling**, driven by a POSIX per-process timer (`timer_create`, `CLOCK_MONOTONIC`) that fires a `SIGUSR1` signal on a fixed 1 ms interval
- **Kernel-assisted context switching** — the signal handler swaps the interrupted thread's `ucontext_t.uc_mcontext`, so the kernel's `sigreturn` restores the full register file (general-purpose, SP, PC, CPSR, and FP/SIMD registers) rather than relying on a hand-rolled save/restore
- **Guarded, independently mapped stacks** — each thread gets its own `mmap`'d region, with the lower page marked `PROT_NONE` via `mprotect` as a guard page against stack overflow
- **Circular doubly linked scheduler queue**, with the calling ("main") thread lazily wrapped into the queue on the first `noodles_create` call
- **Custom `sleep()` override** (`src/common.c`) that re-issues `nanosleep` with the remaining time whenever it's interrupted, so signal delivery doesn't shorten a thread's requested sleep

## Public API

Defined in `include/noodles.h`:

| Function | Description |
|---|---|
| `int noodles_create(nthread_t *nthread, void *(*t_func)(void *), void *tf_arg)` | Creates a new user thread running `t_func(tf_arg)` and adds it to the scheduler queue. Lazily initializes the scheduler and timer on the first call. |
| `int noodles_join(nthread_t *nthread)` | Blocks the caller until the given thread finishes, by repeatedly signaling the scheduler until the thread's state becomes `FINISHED`. |
| `int noodles_yield(void)` | Voluntarily gives up the CPU and invokes the scheduler. |
| `int noodles_exit(void)` | Terminates the calling thread and removes it from the scheduler queue. Also invoked automatically when a thread function returns. |

### Thread states

Defined in `include/_noodles.h`: `RUNNABLE`, `RUNNING`, `BLOCKED`, `FINISHED`. `BLOCKED` is currently defined but not yet set anywhere in the scheduler logic.

## Design notes

- Whenever the first thread creation is invoked, the library shall make 2 threads: (1) a main thread that is responsible for program after the thread creation (2) the actual thread which shall execute the thread function supplied to the creation API. the main thread is created only once in the entire life of the user program and shall retain its postion in the scheduler list even when there are no more user created threads. This will ensure there is o repaeted work whenever the user programs thread count goes from 1 to 0 to 1. 
- The preemption interval is fixed at 1 ms (`1,000,000` ns) and is set up via `timer_create`/`timer_settime`. The timer is disabled while a context switch is in progress and re-armed just before resuming the next thread, so every thread sees a uniform preemption interval.
- An earlier approach with hand-rolled the context save/restore in assembly was attempted, but FP/SIMD registers couldn't be restored that way hence the current design instead lets the kernel restore the full context via `sigreturn` from inside the signal handler.
- When a thread runs for the first time, its stack pointer is set up and its function entered directly through inline AArch64 assembly (`mov sp, ...; br x1`), with `noodles_exit` pre-loaded into the link register (`x30`) so a thread function that simply returns still triggers proper cleanup.
- The stack pointer is adjusted down by 16 bytes before use to satisfy AArch64's mandatory 16-byte stack alignment.
- Each thread's stack is a 2-page `mmap` region: the lower page is the `PROT_NONE` guard page, and the upper page is the usable stack (size = `sysconf(_SC_PAGESIZE)`).

## Project layout

```
noodles/
├── include/
│   ├── noodles.h    # public API
│   ├── _noodles.h   # internal thread struct, thread states, stack sizing
│   └── common.h     # declares the custom sleep()
├── src/
│   ├── noodles.c    # scheduler, context switching, public API implementation
│   └── common.c     # sleep() override
└── Makefile
```

## Building

Requires an AArch64 Linux environment (the thread bootstrap routine is written in AArch64 inline assembly) with `gcc` and `make`.

```sh
make build   # compile everything into build/ and bin/
make all     # build, then run the resulting bin/noodles binary (default target)
make clean   # remove build/ and bin/
make help    # list Makefile targets
```

Note: the `build`, `clean`, and directory-creation targets in the current Makefile run via `sudo`.

The repository currently exposes the library's source and headers under `include/`/`src/`; yet to include an example `main()`/driver program.
## Status

Early-stage and under active development.
