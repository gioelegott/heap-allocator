#ifndef SBRK_LOCK_H
#define SBRK_LOCK_H

/**
 * @file sbrk_lock.h
 * @brief Conditional mutex support shared by all sbrk-backed allocators.
 *
 * Thread safety is enabled by default.  To compile it out entirely, define
 * @c THREAD_SAFE=0 (e.g. pass @c -DTHREAD_SAFE=0 to the compiler, or use
 * @c make @c THREAD_SAFE=false).
 *
 * Each translation unit that needs a mutex must place @c DECLARE_SBRK_MUTEX
 * at file scope exactly once to instantiate the static mutex object.  The
 * @c SBRK_LOCK() and @c SBRK_UNLOCK() macros then refer to that object.
 */

#ifndef THREAD_SAFE
/** @brief 1 enables mutex locking (default); 0 compiles all locking out. */
#define THREAD_SAFE 1
#endif

#if THREAD_SAFE
#  include <pthread.h>
/** @brief Declares a file-local pthread mutex initialised to its default state. */
#  define DECLARE_SBRK_MUTEX \
       static pthread_mutex_t sbrk_mutex = PTHREAD_MUTEX_INITIALIZER;
/** @brief Acquires the file-local sbrk mutex. */
#  define SBRK_LOCK()   pthread_mutex_lock(&sbrk_mutex)
/** @brief Releases the file-local sbrk mutex. */
#  define SBRK_UNLOCK() pthread_mutex_unlock(&sbrk_mutex)
#else
/** @brief No-op when THREAD_SAFE=0: mutex is not declared. */
#  define DECLARE_SBRK_MUTEX
/** @brief No-op when THREAD_SAFE=0. */
#  define SBRK_LOCK()   ((void)0)
/** @brief No-op when THREAD_SAFE=0. */
#  define SBRK_UNLOCK() ((void)0)
#endif

#endif /* SBRK_LOCK_H */
