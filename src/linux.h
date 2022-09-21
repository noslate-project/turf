#ifndef _TURF_LINUX_H_
#define _TURF_LINUX_H_

#include <time.h>  // sturct timeval

/* capacity produced so dependency of libcap,
 * so need to be implemented inside turf.
 */
// #include <sys/capability.h>
#include <linux/filter.h>
#include <sys/prctl.h>
#include <sys/signalfd.h>  // signalfd()
#include <sys/time.h>      // struct timeval
#include <syscall.h>

struct rusage {
  struct timeval ru_utime; /* user time used */
  struct timeval ru_stime; /* system time used */
  long ru_maxrss;          /* max resident set size, in kilo bytes */
  long ru_ixrss;   /* integral shared text memory size, unused on Linux */
  long ru_idrss;   /* integral unshared data size, unused on Linux*/
  long ru_isrss;   /* integral unshared stack size, unused on Linux */
  long ru_minflt;  /* page reclaims, hit in cache/momeory but IO */
  long ru_majflt;  /* page faults, missed and require IO */
  long ru_nswap;   /* swaps, unmaintained on Linux */
  long ru_inblock; /* the number of times the filesystem had to perform input */
  long
      ru_oublock; /* the number of times the filesystem had to perform output */
  long ru_msgsnd; /* messages sent, unused on Linux */
  long ru_msgrcv; /* messages received, unused on Linux*/
  long ru_nsignals; /* signals received, unused on Linux */
  long ru_nvcsw;    /* voluntary context switches */
  long ru_nivcsw;   /* involuntary context switches */
};

// wait4() has resource usage
pid_t wait4(pid_t pid, int* stat_loc, int options, struct rusage* rusage);

// a new chroot
int pivot_root(const char* new_root, const char* put_old);

#endif  // _TURF_LINUX_H_
