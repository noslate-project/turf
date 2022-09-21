#ifndef _TURF_INC_H_
#define _TURF_INC_H_

// turf needs GNU APIs
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// POSIX headers
// #include <inttypes.h>       // strtoumax()
#include <assert.h>   // assert()
#include <dirent.h>   // opendir(), readdir() ...
#include <dlfcn.h>    // dlopen()
#include <errno.h>    // errno
#include <fcntl.h>    // open(), fcntl() ...
#include <getopt.h>   // getopt_long()
#include <sched.h>    // sched_yield()
#include <signal.h>   // signal(), kill() ...
#include <stdarg.h>   // va_arg(), va_start() ...
#include <stdbool.h>  // bool
#include <stdint.h>   // uint32_t, uint8_t ...
#include <stdio.h>    // fopen(), printf(),  ...
#include <stdlib.h>   // malloc(), getenv(), abort() ...
#include <string.h>   // memcpy(), strdup(), strcmp() ...
#include <time.h>     // clock_gettime()
#include <unistd.h>   // fork(), access(), read() ...

#include <sys/mount.h>  // mount()
#include <sys/stat.h>   // stat(), fstat(), lstat() ...
#include <sys/types.h>  // pid_t, size_t ...
#include <sys/wait.h>   // wait(), waitpid() ...

#include "queue.h"  // override the sys/queue.h

// last, load turf config setting
#include "config.h"

#if defined(__linux__)
#include "linux.h"
#endif

#if defined(__APPLE__)
#undef USE_SYSADMIN  // not support under macos
#include "darwin.h"
#endif

#endif  // _TURF_INC_H_
