#ifndef UTILITY_TIME_H
#define UTILITY_TIME_H

#include <signal.h>
#include <stdint.h>
#include <sys/select.h>
#include <sys/time.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline uint64_t GetCurrentTimeStamp()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000000 + tv.tv_usec;
}

static inline void SelectSleep(int microseconds)
{
    struct timeval delay = {
        .tv_sec = 0,
        .tv_usec = microseconds,
    };
    select(0, NULL, NULL, NULL, &delay);
}

static inline void NanoSleep(int nanoseconds)
{
    struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = nanoseconds,
    };
    nanosleep(&ts, NULL);
}

static inline void MicroSleep(int microseconds)
{
    struct timespec ts = {
        .tv_sec = 0,
        .tv_nsec = microseconds * 1000,
    };
    nanosleep(&ts, NULL);
}

static inline void InitTimer(int32_t second, void(*timerProc))
{
    struct sigaction act;
    act.sa_handler = timerProc;
    act.sa_flags = 0;
    sigemptyset(&act.sa_mask);
    sigaction(SIGALRM, &act, NULL);
    struct itimerval val;
    val.it_value.tv_sec = second;
    val.it_value.tv_usec = 0;
    val.it_interval = val.it_value;
    setitimer(ITIMER_REAL, &val, NULL);
}

#ifdef __cplusplus
}
#endif

#endif  // UTILITY_TIME_H