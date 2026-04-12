#ifndef TIMER_H
#define TIMER_H

#include <time.h>

/**
 * @brief Opaque timer handle holding a start timestamp.
 */
typedef struct {
    struct timespec start; /**< Timestamp captured by timer_start(). */
} prof_timer_t;

/**
 * @brief Record the current monotonic time into @p t.
 *
 * @param t Pointer to the timer to initialise.
 * @return void
 */
static inline void timer_start(prof_timer_t *t)
{
    clock_gettime(CLOCK_MONOTONIC, &t->start);
}

/**
 * @brief Return elapsed time since timer_start() in milliseconds.
 *
 * @param t Pointer to a previously started timer.
 * @return Elapsed time in milliseconds (double).
 */
static inline double timer_elapsed_ms(const prof_timer_t *t)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (now.tv_sec  - t->start.tv_sec)  * 1e3
         + (now.tv_nsec - t->start.tv_nsec) * 1e-6;
}

#endif
