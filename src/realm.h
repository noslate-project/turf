#ifndef _TURF_REALM_H_
#define _TURF_REALM_H_

#include "misc.h"

#define RLM_DEF_UID 36767
#define RLM_DEF_GID 36767
#define RLM_DEF_CAPB 0x0
#define RLM_DEF_CGROUP_DIR "/sys/fs/cgroup"

#define RLM_MODE_FALLBACK 0  // without any priviliage
#define RLM_MODE_SYSADMIN 1  // full cap_sysadmin support

#define RLM_STATE_UNKNOWN 0   // unknown state, broken?
#define RLM_STATE_INIT 1      // after craete
#define RLM_STATE_STARTING 2  // bring up the realm
#define RLM_STATE_RUNNING 3   // running
#define RLM_STATE_STOPPING 4  // tear down the realm, but not exit yet
#define RLM_STATE_STOPPED 5   // stopped
#define RLM_STATE_KILLING 6   // kill in progress
#define RLM_STATE_FORKWAIT 7  // can be forked
#define RLM_STATE_CLONING 8   // under cloning, both in seed and clone process
#define RLM_STATE_CLONED 9    // hole new cloned process

#define RLM_STATUS_OK (0)            // all good
#define RLM_STATUS_MEM_OVL (1 << 0)  // last mem overload
#define RLM_STATUS_CPU_OVL (1 << 1)  // last cpu overload
#define RLM_STATUS_KILL (1 << 2)     // last killed
#define RLM_STATUS_EXITED (1 << 3)   // no longer running

// realm configurations
struct rlm_cfg {
  struct {
    int uid : 1;            // enable uid
    int gid : 1;            // enable gid
    int cpulimit : 1;       // enable cpu limit
    int memlimit : 1;       // enable mem limit
    int chroot : 1;         // change root dir
    int capbset : 1;        // set capabilities mask
    int ns_mount : 1;       // new mount points namespace
    int ns_pid : 1;         // new pid namespace
    int mount_tmp : 1;      // mount tmpfs
    int mount_proc_ro : 1;  // mount proc readonly
    int terminal : 1;       // need a terminal
    int fd_stdout : 1;      // redirect stdout to fd
    int fd_stderr : 1;      // redirect stderr to fd
    int socketpair : 1;     // enable socketpair
  } flags;

  int mode;  // RLM_MODE_XXX

  int argc;     // exec arguments count
  char** argv;  // exec arguments

  char** env;    // exec environ
  char* binary;  // realpath of binary
  char* name;    // sandbox name

  uint32_t memlimit;  // memory limit in MB.
  uint32_t cpulimit;  // cpu limit in percent.

  uid_t uid;  // uid and gid
  gid_t gid;

  char* chroot_dir;                           // change root dir "/"
  uint64_t capbset_mask;                      // capabilities mask
  SLIST_HEAD(mounts_hdr, mountpoint) mounts;  // mountpoints

  char* fd_stdout;  // stdout redirect
  char* fd_stderr;  // stderr redirect

  int sv[2];  // socketpair
};

// realm states
struct rlm_st {
  pid_t child_pid;          // child pid
  uint32_t state;           // RLM_STATE_XXX, running, stopped ...
  uint32_t status;          // RLM_STATUS_XXX, OOM, killed ...
  struct timeval tv_start;  // start time
  struct timeval tv_stop;   // stopped time
  int exit_code;            // exit code, from wait4()
  struct rusage exit_ruse;  // resource usage, from wait4()
};

// represent a sandbox realm
struct rlm_t {
  struct rlm_cfg cfg;
  struct rlm_st st;
};

// mount point
struct mountpoint {
  char* src;
  char* dest;
  char* type;
  char* data;
  int has_data;
  uint64_t flags;
  SLIST_ENTRY(mountpoint) next;
};

// realm APIs
struct rlm_t _API* rlm_new(const char* name);  // new a realm (sandbox)
int _API rlm_run(struct rlm_t* r);             // run the realm
int _API rlm_wait(struct rlm_t* r,
                  int* ec,
                  struct rusage* ru);         // wait the realm to finish
int _API rlm_enter(struct rlm_t* r);          // enter the realm
int _API rlm_kill(struct rlm_t* r, int sig);  // using signal to kill the realm
void _API rlm_free_inner(struct rlm_t* r);    // free realm context
void _API rlm_free(struct rlm_t* r);          // free a stopped realm
int _API rlm_handle_exit(struct rlm_t* r,
                         int ec,
                         struct rusage* ru);  // handle rlm exit
const char _API* rlm_state_str(int state);    // return state string
uint32_t _API rlm_state(const char* str);     // state string to enum
int _API rlm_fork(struct rlm_t* r);           // fork the rlm

// adjust the realm
int _API rlm_setuid(struct rlm_t* r, uid_t uid);
int _API rlm_setgid(struct rlm_t* r, uid_t uid);
int _API rlm_capbset(struct rlm_t* r, uint64_t mask);
int _API rlm_limit_mem(struct rlm_t* r, uint32_t mem);
int _API rlm_limit_cpu(struct rlm_t* r, uint32_t cpu);
int _API rlm_mount(struct rlm_t* r,
                   const char* src,
                   const char* dest,
                   const char* type,
                   uint64_t flags);
int _API rlm_chroot(struct rlm_t* r, const char* dir);
int _API rlm_env(struct rlm_t* r, char** env);
int _API rlm_arg(struct rlm_t* r, int argc, char* argv[]);
int _API rlm_binary(struct rlm_t* r, char* path);
int _API rlm_socketpair(struct rlm_t* r, int sv[2]);

#endif  //_TURF_REALM_H_
