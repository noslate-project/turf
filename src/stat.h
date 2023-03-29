#ifndef _TURF_STAT_H_
#define _TURF_STAT_H_

#include "misc.h"

#define PID_IO "/proc/%d/io"
#define PID_STAT "/proc/%d/stat"

struct tf_stat {
  /*
  /proc/#pid/io, IO statistics for each running process.

  rchar
  I/O counter: chars read
  The number of bytes which this task has caused to be read from storage. This
  is simply the sum of bytes which this process passed to read() and pread().
  It includes things like tty IO and it is unaffected by whether or not actual
  physical disk IO was required (the read might have been satisfied from
  pagecache).

  wchar
  I/O counter: chars written
  The number of bytes which this task has caused, or shall cause to be written
  to disk. Similar caveats apply here as with rchar.

  syscr
  I/O counter: read syscalls
  Attempt to count the number of read I/O operations, i.e. syscalls like read()
  and pread().

  syscw
  I/O counter: write syscalls
  Attempt to count the number of write I/O operations, i.e. syscalls like
  write() and pwrite().

  read_bytes
  I/O counter: bytes read
  Attempt to count the number of bytes which this process really did cause to
  be fetched from the storage layer. Done at the submit_bio() level, so it is
  accurate for block-backed filesystems. <please add status regarding NFS and
  CIFS at a later time>

  write_bytes
  I/O counter: bytes written
  Attempt to count the number of bytes which this process caused to be sent to
  the storage layer. This is done at page-dirtying time.

  cancelled_write_bytes
  The big inaccuracy here is truncate. If a process writes 1MB to a file and
  then deletes the file, it will in fact perform no writeout. But it will have
  been accounted as having caused 1MB of write.
  In other words: The number of bytes which this process caused to not happen,
  by truncating pagecache. A task can cause "negative" IO too. If this task
  truncates some dirty pagecache, some IO which another task has been accounted
  for (in its write_bytes) will not be happening. We _could_ just subtract that
  from the truncating task's write_bytes, but there is information loss in doing
  that.

  .. Note::
     At its current implementation state, this is a bit racy on 32-bit machines:
     if process A reads process B's /proc/pid/io while process B is updating one
     of those 64-bit counters, process A could see an intermediate result.
  */
  unsigned long long rchar;                  // read() bytes
  unsigned long long wchar;                  // write() bytes
  unsigned long long syscr;                  // read() syscall count
  unsigned long long syscw;                  // write() syscall count
  unsigned long long read_bytes;             // read storage bytes
  unsigned long long write_bytes;            // write storage bytes
  unsigned long long cancelled_write_bytes;  // write to pagecahe but not write
                                             // to storage bytes

  /*
  /proc/#pid/stat, process status
  == =============
  =============================================================== No Field
  Content
  == =============
  =============================================================== 1  pid process
  id 2  tcomm         filename of the executable 3  state         state (R is
  running, S is sleeping, D is sleeping in an uninterruptible wait, Z is zombie,
  T is traced or stopped) 4  ppid          process id of the parent process 5
  pgrp          pgrp of the process 6  sid           session id 7  tty_nr tty
  the process uses 8  tty_pgrp      pgrp of the tty 9  flags         task flags
  10 min_flt       number of minor faults
  11 cmin_flt      number of minor faults with child's
  12 maj_flt       number of major faults
  13 cmaj_flt      number of major faults with child's
  14 utime         user mode jiffies
  15 stime         kernel mode jiffies
  16 cutime        user mode jiffies with child's
  17 cstime        kernel mode jiffies with child's
  18 priority      priority level
  19 nice          nice level
  20 num_threads   number of threads
  21 it_real_value (obsolete, always 0)
  22 start_time    time the process started after system boot
  23 vsize         virtual memory size
  24 rss           resident set memory size
  25 rsslim        current limit in bytes on the rss
  26 start_code    address above which program text can run
  27 end_code      address below which program text can run
  28 start_stack   address of the start of the main process stack
  29 esp           current value of ESP
  30 eip           current value of EIP
  31 pending       bitmap of pending signals
  32 blocked       bitmap of blocked signals
  33 sigign        bitmap of ignored signals
  34 sigcatch      bitmap of caught signals
  35 0             (place holder, used to be the wchan address,
                   use /proc/PID/wchan instead)
  36 0             (place holder)
  37 0             (place holder)
  38 exit_signal   signal to send to parent thread on exit
  39 task_cpu      which CPU the task is scheduled on
  40 rt_priority   realtime priority
  41 policy        scheduling policy (man sched_setscheduler)
  42 blkio_ticks   time spent waiting for block IO
  43 gtime         guest time of the task in jiffies
  44 cgtime        guest time of the task children in jiffies
  45 start_data    address above which program data+bss is placed
  46 end_data      address below which program data+bss is placed
  47 start_brk     address above which program heap can be expanded with brk()
  48 arg_start     address above which program command line is placed
  49 arg_end       address below which program command line is placed
  50 env_start     address above which program environment is placed
  51 env_end       address below which program environment is placed
  51 exit_code     the thread's exit_code in the form reported by the waitpid
                   system call
  == =============
  ===============================================================
  */
  pid_t pid;   // id of the process
  pid_t ppid;  // parent pid
  pid_t pgid;  // process group id, pgid == pid, as the group leader
  pid_t sid;   // session id, sid == pid, as the session leader

  unsigned long min_flt;   // minor page faults, hit in cache or memory
  unsigned long maj_flt;   // major page faults, load from storage
  unsigned long cmin_flt;  // minor page faults, with all children
  unsigned long cmaj_flt;  // major page faults, with all children

  unsigned long stime;   // kernel time in MS
  unsigned long utime;   // user time in MS
  unsigned long cstime;  // kernel time with all children
  unsigned long cutime;  // user time with all children

  unsigned long vsize;  // virtual memory size, in KBs
  unsigned long rss;    // physical memory size, in KBs

  int num_threads;  // number of threads
};

typedef struct tf_stat tf_stat;

int _API pid_stat(pid_t pid, tf_stat* stat);
void _API dump_stat(tf_stat* stat);

#endif  // _TURF_STAT_H_
