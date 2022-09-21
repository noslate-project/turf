#include "stat.h"

// get io_stat from procfs
static int proc_io(pid_t pid, tf_stat* stat) {
#define BUF_MAX 1024
  char buf[BUF_MAX] = {0};
  int fd = 0;
  int rc = 0;

  if (!stat || pid <= 0) {
    set_errno(EINVAL);
    return -1;
  }

  snprintf(buf, BUF_MAX, PID_IO, pid);
  fd = open(buf, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }

  rc = read(fd, buf, BUF_MAX);
  if (rc <= 0) {
    close(fd);
    return rc;
  }

  rc = sscanf(buf,
              "rchar: %llu\n"
              "wchar: %llu\n"
              "syscr: %llu\n"
              "syscw: %llu\n"
              "read_bytes: %llu\n"
              "write_bytes: %llu\n"
              "cancelled_write_bytes: %llu\n",
              &stat->rchar,
              &stat->wchar,
              &stat->syscr,
              &stat->syscw,
              &stat->read_bytes,
              &stat->write_bytes,
              &stat->cancelled_write_bytes);

  if (rc == 7) {  // good
    rc = 0;
  } else {
    error("proc.io.pid(%d).scan feiled %d", pid, rc);
    rc = -1;
  }

  close(fd);
  return rc;
#undef BUF_MAX
}

// get pid_stat from procfs
int proc_stat(pid_t pid, tf_stat* stat) {
#define BUF_MAX 1024
  char buf[BUF_MAX] = {0};
  char* array[64] = {0};
  int fd = 0;
  int rc = 0;
  int i = 0;
  char* p;

  if (!stat || pid <= 0) {
    set_errno(EINVAL);
    return -1;
  }

  snprintf(buf, BUF_MAX, PID_STAT, pid);
  fd = open(buf, O_RDONLY | O_CLOEXEC);
  if (fd < 0) {
    return -1;
  }
  rc = read(fd, buf, BUF_MAX);
  if (rc < 0) {
    return rc;
  }

  p = strtok(buf, " ");
  while (p) {
    if (i > (int)(ARRAY_SIZE(array) - 1)) {
      break;
    }
    array[i++] = p;
    p = strtok(NULL, " ");
  }
  close(fd);

#if 0 
    struct {
        const char *name;
        int idx;
    } cl[] = {
        {"pid", 0},
        {"cmd", 1},
        {"state", 2},
        {"ppid", 3},
        {"pgid", 4},
        {"sid", 5},
        {"flags", 8},
        {"min_flt", 9},
        {"cmin_flt", 10},
        {"maj_flt", 11},
        {"cmaj_flt", 12},
        {"utime", 13},
        {"stime", 14},
        {"cutime", 15},
        {"cstime", 16},
        {"priority", 17},
        {"nice", 18},
        {"num_threads", 19},
        {"start_time", 21},
        {"vsize", 22},
        {"rss", 23},
        {"rsslimit", 24},
    };
    
    for(i=0; i<ARRAY_SIZE(cl); i++) {
        printf("%s = %s\n", cl[i].name, array[cl[i].idx]);
    }
#endif

  stat->pid = strtol(array[0], NULL, 10);
  stat->ppid = strtol(array[3], NULL, 10);
  stat->pgid = strtol(array[4], NULL, 10);
  stat->sid = strtol(array[5], NULL, 10);

  stat->min_flt = strtoul(array[9], NULL, 10);
  stat->cmin_flt = strtoul(array[10], NULL, 10);
  stat->maj_flt = strtoul(array[11], NULL, 10);
  stat->cmaj_flt = strtoul(array[12], NULL, 10);

  /* kernel uses jiffies, here changed to MS
   */
  stat->utime = strtoul(array[13], NULL, 10) * 1000 / HZ;
  stat->stime = strtoul(array[14], NULL, 10) * 1000 / HZ;
  stat->cutime = strtoul(array[15], NULL, 10) * 1000 / HZ;
  stat->cstime = strtoul(array[16], NULL, 10) * 1000 / HZ;

  /* kernel task_vsize, PAGESIZE * mm->total_vm
   */
  stat->vsize = strtoul(array[22], NULL, 10) / 1024;

  /* kernel mm_struct, rss is anon_rss + file_rss pages
   */
  stat->rss = strtoul(array[23], NULL, 10) * PAGE_SIZE / 1024;

  // num_threads
  stat->num_threads = strtoul(array[19], NULL, 10);
  return 0;
#undef BUF_MAX
}

// get the pid's stat
int _API pid_stat(pid_t pid, tf_stat* stat) {
  int rc;

  if (pid <= 1) {
    set_errno(EINVAL);
    return -1;
  }

  // check pid
  rc = kill(pid, 0);
  if (rc < 0) {
    set_errno(ENOENT);
    return -1;
  }

  rc = proc_stat(pid, stat);
  if (rc < 0) {
    pwarn("proc.stat.pid %d failed", pid);
    // fall through
  }

  rc = proc_io(pid, stat);
  if (rc < 0) {
    pwarn("proc.io.pid %d failed", pid);
    // fall through
  }

  return 0;
}

// dump the tf_stat
void _API dump_stat(tf_stat* stat) {
  dprint("stat info:\n"
         "pid: %d\n"
         "ppid: %d\n"
         "pgid: %d\n"
         "sid: %d\n"
         "min_flt: %lu\n"
         "maj_flt: %lu\n"
         "cmin_flt: %lu\n"
         "cmaj_flt: %lu\n"
         "stime: %lu\n"
         "utime: %lu\n"
         "cstime: %lu\n"
         "cutime: %lu\n"
         "vsize: %lu\n"
         "rss: %lu\n"
         "num_threads: %d\n"
         "rchar: %llu\n"
         "wchar: %llu\n"
         "syscr: %llu\n"
         "syswr: %llu\n"
         "read_bytes: %llu\n"
         "write_bytes: %llu\n"
         "cancelled_write_bytes: %llu\n",
         stat->pid,
         stat->ppid,
         stat->pgid,
         stat->sid,
         stat->min_flt,
         stat->maj_flt,
         stat->cmin_flt,
         stat->cmaj_flt,
         stat->stime,
         stat->utime,
         stat->cstime,
         stat->cutime,
         stat->vsize,
         stat->rss,
         stat->num_threads,
         stat->rchar,
         stat->wchar,
         stat->syscr,
         stat->syscw,
         stat->read_bytes,
         stat->write_bytes,
         stat->cancelled_write_bytes);
}
