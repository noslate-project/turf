/* realm implementation.
 *   realm is a process sandbox.
 */

#include "realm.h"

#if defined(USE_SYSADMIN)
#include "realm_sysadmin.c"
#endif

#if defined(USE_FALLBACK)
#include "realm_fallback.c"
#endif

// APIs for a turf sandbox
// all setting only goes after rlm_enter().

// setting uid
int _API rlm_setuid(struct rlm_t* r, uid_t uid) {
  r->cfg.flags.uid = 1;
  r->cfg.uid = uid;
  return 0;
}

// setting gid
int _API rlm_setgid(struct rlm_t* r, uid_t gid) {
  r->cfg.flags.gid = 1;
  r->cfg.gid = gid;
  return 0;
}

// settting capabilities
int _API rlm_capbset(struct rlm_t* r, uint64_t mask) {
  r->cfg.flags.capbset = 1;
  r->cfg.capbset_mask = mask;
  return 0;
}

// limit memory
int _API rlm_limit_mem(struct rlm_t* r, uint32_t rss) {
  r->cfg.memlimit = rss;
  r->cfg.flags.memlimit = 1;
  return 0;
}

// limit cpu
int _API rlm_limit_cpu(struct rlm_t* r, uint32_t cpu) {
  r->cfg.cpulimit = cpu;
  r->cfg.flags.cpulimit = 1;
  return 0;
}

// setting environ
int _API rlm_env(struct rlm_t* r, char** env) {
  r->cfg.env = sscopy(env);
  return 0;
}

int _API rlm_arg(struct rlm_t* r, int argc, char* argv[]) {
  r->cfg.argc = argc;
  r->cfg.argv = sscopy(argv);
  return 0;
}

int _API rlm_binary(struct rlm_t* r, char* binary) {
  r->cfg.binary = strdup(binary);
  return 0;
}

// mount entry to realm
int _API rlm_mount(struct rlm_t* r,
                   const char* src,
                   const char* dest,
                   const char* type,
                   uint64_t flags) {
  struct mountpoint* m;

  if (*dest != '/') {
    set_errno(EINVAL);
    return -1;
  }
  m = (struct mountpoint*)calloc(1, sizeof(*m));
  if (!m) {
    set_errno(ENOMEM);
    return -1;
  }
  m->dest = strdup(dest);
  if (!m->dest) goto error;
  m->src = strdup(src);
  if (!m->src) goto error;
  m->type = strdup(type);
  if (!m->type) goto error;
  m->flags = flags;

  info("mount %s -> %s type '%s'", src, dest, type);

  // needs new mount namespace
  r->cfg.flags.ns_mount = 1;

  SLIST_INSERT_HEAD(&r->cfg.mounts, m, next);

  return 0;

error:
  free(m->type);
  free(m->src);
  free(m->dest);
  free(m);
  set_errno(ENOMEM);
  return -1;
}

// chroot dir
int _API rlm_chroot(struct rlm_t* r, const char* dir) {
  if (r->cfg.chroot_dir) {
    set_errno(EINVAL);
    return -1;
  }
  r->cfg.chroot_dir = strdup(dir);
  if (!r->cfg.chroot_dir) {
    set_errno(ENOMEM);
    return -1;
  }
  r->cfg.flags.chroot = 1;
  return 0;
}

int _API rlm_terminal(struct rlm_t* r) {
  r->cfg.flags.terminal = 1;
  return 0;
}

// stdout
int _API rlm_stdout(struct rlm_t* r, char* out_file) {
  r->cfg.fd_stdout = strdup(out_file);
  r->cfg.flags.fd_stdout = 1;
  return 0;
}

int _API rlm_stderr(struct rlm_t* r, char* err_file) {
  r->cfg.fd_stderr = strdup(err_file);
  r->cfg.flags.fd_stderr = 1;
  return 0;
}

// create a new realm.
struct rlm_t _API* rlm_new(const char* name) {
  struct rlm_t* r = (struct rlm_t*)calloc(1, sizeof(struct rlm_t));
  if (!r) {
    set_errno(ENOMEM);
    return NULL;
  }
  r->cfg.name = strdup(name);
  r->st.state = RLM_STATE_INIT;
  return r;
}

// config the realm, before transfer to user.
int _API rlm_enter(struct rlm_t* r) {
  set_errno(ENOTSUP);
  return -1;
}

// exec()
int _API rlm_run(struct rlm_t* r) {
  int rc;
  r->st.state = RLM_STATE_STARTING;

#if defined(USE_SYSADMIN)
  if (r->cfg.mode == RLM_MODE_SYSADMIN) {
    rc = sysadmin_run(r);
  } else
#endif
      if (r->cfg.mode == RLM_MODE_FALLBACK) {
    rc = fallback_run(r);
  } else {
    set_errno(ENOTSUP);
    return -1;
  }

  r->st.state = RLM_STATE_RUNNING;
  return rc;
}

// for caller had waitpid()
int rlm_handle_exit(struct rlm_t* r, int ec, struct rusage* ru) {
  r->st.exit_code = ec;
  r->st.exit_ruse = *ru;

#if defined(__APPLE__)
  /* BSD using bytes against Linux's Kilo Bytes.
   */
  r->st.exit_ruse.ru_maxrss /= 1024;
#endif

  r->st.state = RLM_STATE_STOPPED;
  info("realm (%d) exited.", r->st.child_pid);
  rusage_dump(&r->st.exit_ruse);

  return 0;
}

// sync wait a realm to be finished.
int _API rlm_wait(struct rlm_t* r, int* exit_code, struct rusage* ru) {
  if (r->st.child_pid <= 0) {
    set_errno(ECHILD);
    return -1;
  }

  pid_t child;
  while (1) {
    child = wait4(r->st.child_pid, exit_code, 0, ru);
    if (child >= 0) {
      break;
    }
    if (errno != EINTR) {
      return -1;
    }
  }

  rlm_handle_exit(r, *exit_code, ru);
  return child;
}

// kill a realm.
int _API rlm_kill(struct rlm_t* r, int sig) {
  int rc = kill(r->st.child_pid, sig);

  // stopping state
  if (sig == SIGKILL || sig == SIGTERM) {
    r->st.state = RLM_STATE_STOPPING;
  }

  return rc;
}

// return state string
const char _API* rlm_state_str(int state) {
  switch (state) {
    case RLM_STATE_INIT:
      return "init";
      break;

    case RLM_STATE_STARTING:
      return "starting";
      break;

    case RLM_STATE_RUNNING:
      return "running";
      break;

    case RLM_STATE_STOPPING:
      return "stopping";
      break;

    case RLM_STATE_STOPPED:
      return "stopped";
      break;

    case RLM_STATE_FORKWAIT:
      return "forkwait";
      break;

    case RLM_STATE_CLONING:
      return "cloning";
      break;

    case RLM_STATE_CLONED:
      return "cloned";
      break;
  }
  return "unknown";
}

uint32_t _API rlm_state(const char* str) {
  if (strcmp(str, "running") == 0) {
    return RLM_STATE_RUNNING;
  } else if (strcmp(str, "stopped") == 0) {
    return RLM_STATE_STOPPED;
  } else if (strcmp(str, "init") == 0) {
    return RLM_STATE_INIT;
  } else if (strcmp(str, "starting") == 0) {
    return RLM_STATE_STARTING;
  } else if (strcmp(str, "stopping") == 0) {
    return RLM_STATE_STOPPING;
  } else if (strcmp(str, "forkwait") == 0) {
    return RLM_STATE_FORKWAIT;
  } else if (strcmp(str, "cloning") == 0) {
    return RLM_STATE_CLONING;
  } else if (strcmp(str, "cloned") == 0) {
    return RLM_STATE_CLONED;
  }
  return RLM_STATE_UNKNOWN;
}

static int rlm_free_mounts(struct rlm_t* r) {
  set_errno(ENOTSUP);
  return -1;
}

// free the realm back to system.
void _API rlm_free_inner(struct rlm_t* r) {
  if (!r) {
    set_errno(EINVAL);
    return;
  }

  // free cfg
  if (r->cfg.env) {
    ssfree(r->cfg.env);
    r->cfg.env = NULL;
  }

  if (r->cfg.argv) {
    ssfree(r->cfg.argv);
    r->cfg.argv = NULL;
  }
  r->cfg.argc = 0;

  if (r->cfg.binary) {
    free(r->cfg.binary);
    r->cfg.binary = NULL;
  }

  if (r->cfg.chroot_dir) {
    free(r->cfg.chroot_dir);
    r->cfg.chroot_dir = NULL;
  }

  if (r->cfg.fd_stdout) {
    free(r->cfg.fd_stdout);
    r->cfg.fd_stdout = NULL;
  }

  if (r->cfg.fd_stderr) {
    free(r->cfg.fd_stderr);
    r->cfg.fd_stderr = NULL;
  }

  if (r->cfg.name) {
    free(r->cfg.name);
    r->cfg.name = NULL;
  }

  if (!SLIST_EMPTY(&r->cfg.mounts)) {
    rlm_free_mounts(r);
  }

  r->st.state = 0;
}

void _API rlm_free(struct rlm_t* r) {
  if (!r) {
    set_errno(EINVAL);
    return;
  }

  rlm_free_inner(r);
  free(r);
}

// setting socketpair for the sandbox
int _API rlm_socketpair(struct rlm_t* r, int sv[2]) {
  r->cfg.sv[0] = sv[0];
  r->cfg.sv[1] = sv[1];
  r->cfg.flags.socketpair = 1;
  return 0;
}

// read the realm's resource usage
int _API rlm_rusage(struct rlm_t* r) {
  return -1;
}

// check realm's health
int _API rlm_chk_health(struct rlm_t* r) {
  return -1;
}

// realm fork
int _API rlm_fork(struct rlm_t* r) {
  int rc;
  r->st.state = RLM_STATE_CLONING;

#if defined(USE_SYSADMIN)
  if (r->cfg.mode == RLM_MODE_SYSADMIN) {
    rc = sysadmin_run(r);
  } else
#endif
      if (r->cfg.mode == RLM_MODE_FALLBACK) {
    rc = fallback_fork(r);
  } else {
    set_errno(ENOTSUP);
    return -1;
  }

  r->st.state = RLM_STATE_RUNNING;
  return rc;
}
