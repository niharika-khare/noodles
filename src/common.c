#include <time.h>

/**
 * Custom implementation of sleep () as signal interception can change 
 * the number of seconds that a thread has actually been inactive for.
 */
unsigned int sleep (unsigned int seconds) {

    struct timespec req;
    struct timespec rem;

    req.tv_sec = seconds;
    req.tv_nsec = 0;

    while (nanosleep (&req, &rem) == -1) {
        req.tv_sec = rem.tv_sec;
        req.tv_nsec = rem.tv_nsec;
    }
    return 0;
}
