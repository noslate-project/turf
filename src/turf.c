/* turf core runtime implementation
 */

#include "turf.h"
#include "ipc.h"  // tipc_decode
#include "oci.h"
#include "realm.h"
#include "shell.h"
#include "sock.h"  // socketpair
#include "spec.h"
#include "stat.h"

// holds all pids the turf created (running realms).
static LIST_HEAD(list_pid, turf_t) m_pids = LIST_HEAD_INITIALIZER(null);

/* APIs for non-runc func
 */

// free a turf_t object and it's resources
static void tf_free(struct turf_t* tf) {
  if (!tf) {
    return;
  }

  if (tf->realm) {
    rlm_free(tf->realm);
    tf->realm = NULL;
  }

  free(tf);
}

// create a new turf_t object
static struct turf_t* tf_new(const char* name) {
  struct turf_t* tf = (struct turf_t*)calloc(1, sizeof(struct turf_t));
  if (!tf) {
    set_errno(ENOMEM);
    return NULL;
  }

  tf->realm = rlm_new(name);
  if (!tf->realm) {
    set_errno(ENOMEM);
    goto error;
  }

  return tf;

error:
  if (tf) {
    tf_free(tf);
  }
  return NULL;
}

// get realm form pid
static struct turf_t* tf_get_realm(pid_t pid) {
  struct turf_t* t;
  LIST_FOREACH(t, &m_pids, in_list) {
    if (t->pid_ == pid) {
      return t;
    }
  }
  return NULL;
}

static struct turf_t* tf_find_realm(const char* name) {
  struct turf_t* t;
  LIST_FOREACH(t, &m_pids, in_list) {
    if (strcmp(t->name_, name) == 0) {
      return t;
    }
  }
  return NULL;
}

// return true if oom
static bool tf_chk_oom(struct turf_t* tf, tf_stat* stat) {
  dprint("pid:%d, mem:%ld, limit:%d, cont:%d",
         tf->realm->st.child_pid,
         stat->rss,
         tf->realm->cfg.memlimit,
         tf->cont_mem_ovl);
  if (tf->realm->cfg.memlimit > 0 && stat->rss > tf->realm->cfg.memlimit) {
    tf->status |= RLM_STATUS_MEM_OVL;
    tf->cont_mem_ovl++;
    if (tf->cont_mem_ovl > 3) {
      return true;
    }
  } else {
    // clear ovl
    tf->cont_mem_ovl = 0;
  }
  return false;
}

// return trur if cpu overload
static bool tf_chk_cpu_overload(struct turf_t* tf, tf_stat* stat) {
  long cpu_used = (stat->utime + stat->cutime + stat->stime + stat->cstime);
  long limit_time = tf->realm->cfg.cpulimit;

  long last_cpu_used = tf->last_cpu_used;
  tf->last_cpu_used = cpu_used;

  struct timeval last_cpu_time;
  last_cpu_time = tf->last_cpu_time;
  get_current_time(&tf->last_cpu_time);

  uint64_t last_cpu_ms = timeval2ms(&last_cpu_time);
  if (last_cpu_ms == 0) {
    return false;
  }

  uint64_t curr_cpu_ms = timeval2ms(&tf->last_cpu_time);
  uint64_t diff_ms = curr_cpu_ms - last_cpu_ms;

  double diff = (double)(cpu_used - last_cpu_used) * 1000 / (double)diff_ms;

  dprint("pid:%d, cpu:%0.1lf, limit:%ld, cont:%d",
         tf->realm->st.child_pid,
         diff,
         limit_time,
         tf->cont_cpu_ovl);
  if (limit_time > 0 && diff > limit_time) {
    tf->status |= RLM_STATUS_CPU_OVL;
    tf->cont_cpu_ovl++;
    if (tf->cont_cpu_ovl > 15) {
      return true;
    }
  } else {
    // clear ovl
    tf->cont_cpu_ovl = 0;
  }
  return false;
}

// check all pids health
int _API tf_health_check(void) {
  int rc;
  struct turf_t* tf;
  struct turf_t* save;

  // wall all the sandboxies
  LIST_FOREACH_SAFE(tf, &m_pids, in_list, save) {
    tf_stat stat = {0};

    // pid
    if (tf->pid_ <= 0) {
      continue;
    }

    if (tf->status & RLM_STATUS_EXITED) {
      LIST_REMOVE(tf, in_list);
      tf_free(tf);
      continue;
    }

    // get state
    rc = pid_stat(tf->pid_, &stat);
    if (rc < 0) {
      pwarn("pid_stat(%d) failed.", tf->pid_);
      continue;
    }

    // check mem usage
    if (tf_chk_oom(tf, &stat)) {
      warn("%d reaches the mem limit", tf->pid_);
      rlm_kill(tf->realm, SIGKILL);
      tf->status |= RLM_STATUS_KILL;
    }

    // check cpu usage
    if (tf_chk_cpu_overload(tf, &stat)) {
      warn("%d reaches the cpu limit", tf->pid_);
      rlm_kill(tf->realm, SIGKILL);
      tf->status |= RLM_STATUS_KILL;
    }

    tf->stat = stat;
  }
  return 0;
}

// handle child exit signal
int _API tf_child_exit(pid_t child, struct rusage* ru, int exit_code) {
  char dest[TURF_MAX_PATH_LEN];
  struct turf_t* tf = tf_get_realm(child);
  if (tf) {
    // LIST_REMOVE(tf, in_list);
    const char* name = tf->realm->cfg.name;

    shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
    struct oci_state* state = oci_state_load(dest);
    if (!state) {
      error("state for sandbox not found.");
      return -1;
    }

    state->state = RLM_STATE_STOPPED;  // stop
    state->status = tf->status;        // oom, cpu_ovl, killed ...

    // save exit code
    state->exit_code = exit_code;
    state->has.exit_code = 1;

    // save rusage
    state->rusage = *ru;
    state->has.rusage = 1;

    // save last stat
    state->stat = tf->stat;
    state->has.stat = 1;

    oci_state_save(state, dest);
    oci_state_free(state);

    tf->status |= RLM_STATUS_EXITED;
    // rlm_free(tf->realm);
    // tf->realm = NULL;
  }
  return 0;
}

// exit all pids
int _API tf_exit_all() {
  set_errno(ENOTSUP);
  return -1;
}

static int tf_cfg_update_spec(struct tf_cli* cfg, struct oci_spec* spec) {
  if (!cfg || !spec) {
    set_errno(EINVAL);
    return -1;
  }

  // Megabytes to bytes
  if (cfg->has.memlimit) {
    spec->linuxs.resources.mem_limit = ((int64_t)cfg->memlimit) * 1024 * 1024;
    spec->linuxs.resources.has.mem_limit = 1;

    spec->linuxs.has.resources = 1;
    spec->has.linuxs = 1;
  }

  // cpu period sets to 1second
  if (cfg->has.cpulimit) {
    spec->linuxs.resources.cpu_period = 1000000;
    spec->linuxs.resources.cpu_quota = cfg->cpulimit * 10000;
    spec->linuxs.resources.has.cpu_period = 1;
    spec->linuxs.resources.has.cpu_quota = 1;

    spec->linuxs.has.resources = 1;
    spec->has.linuxs = 1;
  }

  return 0;
}

int _API tf_find_binary(char** binary, const char* runtime, const char* arg) {
  int i;
  if (arg[0] == '/') {  // realpath
    *binary = strdup(arg);
    // TBD: check exists.
    return 0;
  }
  const char* paths[] = {".", "bin", "sbin", "usr/bin", "usr/sbin"};
  for (i = 0; i < (sizeof(paths) / sizeof(char*)); i++) {
    if (shl_exist4(tfd_path_runtime(), runtime, paths[i], arg)) {
      xasprintf(
          binary, "%s/%s/%s/%s", tfd_path_runtime(), runtime, paths[i], arg);
      dprint("binary: %s", *binary);
      return 0;
    }
  }

  set_errno(ENOENT);
  return -1;
}

// send seed msg
static int tf_seed_fork_req(struct turf_t* tf, struct rlm_t* r) {
  char buf[4096];

  int len = tipc_enc_fork_req(buf, 4096, r);
  dprint("tipc msg size: %d", len);

  int l = write(tf->realm->cfg.sv[0], buf, len);
  if (l != len) {
    error("send fork_req failed");
    return -1;
  }

  return 0;
}

// received seed msg
static int tf_seed_msg(int type, char* buff, size_t size, void* data) {
  dprint("%s: %s", __func__, tipc_msg_type(type));
  switch (type) {
    case TIPC_MSG_SEED_READY: {
      struct turf_t* tf = (struct turf_t*)data;
      char dest[TURF_MAX_PATH_LEN];
      shl_path3(
          dest, sizeof(dest), tfd_path_sandbox(), tf->realm->cfg.name, "state");
      struct oci_state* state = oci_state_load(dest);
      tf->realm->st.state = RLM_STATE_FORKWAIT;
      state->state = tf->realm->st.state;  // forkwait
      oci_state_save(state, dest);
      oci_state_free(state);
      state = NULL;
    } break;

    case TIPC_MSG_FORK_RSP: {
      struct rlm_t r = {0};
      int rc = tipc_dec_fork_rsp(&r, buff, size);

      struct turf_t* tf = tf_find_realm(r.cfg.name);
      if (!tf) {
        error("tf %s not found.", r.cfg.name);
        break;
      }
      tf->realm->st.child_pid = r.st.child_pid;
      tf->realm->st.state = RLM_STATE_RUNNING;

      char dest[TURF_MAX_PATH_LEN];
      shl_path3(
          dest, sizeof(dest), tfd_path_sandbox(), tf->realm->cfg.name, "state");
      struct oci_state* state = oci_state_load(dest);
      state->pid = tf->realm->st.child_pid;  // forkwait
      state->state = tf->realm->st.state;    // forkwait
      oci_state_save(state, dest);
      oci_state_free(state);
      state = NULL;
    } break;
  }
  return 0;
}

static void tf_seed_on_read(struct sck_loop* loop,
                            int fd,
                            void* clientData,
                            int mask) {
  char msg[4096];
  int rc;
  rc = read(fd, msg, 4096);
  if (rc < 0) {
    perror("tf_seed_on_read");
  }
  info("rc=%d", rc);

  tipc_decode(msg, rc, tf_seed_msg, clientData);
}

/* APIs for runc-mode func.
 */

// api for create '~/.turf' workdir.
static int tf_do_init(struct tf_cli* cfg) {
  shl_mkdir2(tfd_path(), "");
  shl_mkdir2(tfd_path(), "sandbox");
  shl_mkdir2(tfd_path(), "runtime");
  shl_mkdir2(tfd_path(), "overlay");
  return 0;
}

// generate oci-spec config.json
static int tf_do_spec(struct tf_cli* cfg) {
  int rc = -1;
  struct oci_spec* spec = NULL;

  // load default spec
  spec = oci_spec_loads(get_spec_json());
  if (!spec) {
    goto exit;
  }

  // update spec using cli value
  rc = tf_cfg_update_spec(cfg, spec);
  if (rc < 0) {
    goto exit;
  }

  // save to file according to bundle_path
  char dest[TURF_MAX_PATH_LEN];
  if (cfg->bundle_path) {
    shl_path2(dest, TURF_MAX_PATH_LEN, cfg->bundle_path, "config.json");
  } else {
    shl_path2(dest, TURF_MAX_PATH_LEN, "./", "config.json");
  }
  rc = oci_spec_save(spec, dest);
  if (rc < 0) {
    goto exit;
  }

exit:
  if (spec) {
    oci_spec_free(spec);
  }
  return rc;
}

// api for list all turf realm
static int tf_do_list(struct tf_cli* cfg) {
  DIR* dir;
  struct dirent* ino;
  dir = opendir(tfd_path_sandbox());
  if (!dir) {
    die("opendir");
  }
  while ((ino = readdir(dir)) != NULL) {
    if (ino->d_type & DT_DIR && strcmp(ino->d_name, ".") &&
        strcmp(ino->d_name, "..")) {
      printf("%s\n", ino->d_name);
    }
  }
  closedir(dir);
  return 0;
}

// api for create a turf realm
static int tf_do_create(struct tf_cli* cfg) {
  const char* name = cfg->sandbox_name;
  bool success = 0;

  char bundle_config_path[TURF_MAX_PATH_LEN];
  char bundle_code_path[TURF_MAX_PATH_LEN];
  if (cfg->bundle_path) {
    shl_path2(bundle_config_path,
              sizeof(bundle_config_path),
              cfg->bundle_path,
              "config.json");
    shl_path2(
        bundle_code_path, sizeof(bundle_code_path), cfg->bundle_path, "code");
  } else {
    shl_path2(
        bundle_config_path, sizeof(bundle_config_path), "./", "config.json");
    shl_path2(bundle_code_path, sizeof(bundle_code_path), "./", "code");
  }

  struct oci_spec* spec = oci_spec_load(bundle_config_path);
  if (!spec) {
    error("spec not found");
    set_errno(EINVAL);
    return -1;
  }

  // check if exists
  if (!shl_notexist2(tfd_path_sandbox(), name)) {
    error("sandbox exists");
    goto exit;
  }

  if (!shl_notexist2(tfd_path_overlay(), name)) {
    error("overlay exists");
    goto exit;
  }

  if (!spec->has.turf) {
    error("spec error");
    goto exit;
  }

  if (!spec->turf.has.runtime || !spec->turf.runtime ||
      shl_notexist2(tfd_path_runtime(), spec->turf.runtime)) {
    error("runtime not found %s", spec->turf.runtime);
    goto exit;
  }

  // makeup dirs
  shl_mkdir2(tfd_path_sandbox(), name);
  shl_mkdir2(tfd_path_overlay(), name);
  shl_mkdir3(tfd_path_overlay(), name, "work");
  shl_mkdir3(tfd_path_overlay(), name, "data");

  // copy config
  char dest[TURF_MAX_PATH_LEN];
  shl_path2(dest, sizeof(dest), tfd_path_sandbox(), name);
  shl_cp(bundle_config_path, dest, "config.json");

  // copy code
  shl_path2(dest, sizeof(dest), tfd_path_overlay(), name);
  shl_cp(bundle_code_path, dest, "code");

  success = 1;

exit:
  if (spec) {
    oci_spec_free(spec);
  }
  return success ? 0 : -1;
}

// delete a sandbox
static int tf_do_delete(struct tf_cli* cfg) {
  int rc = -1;
  char dest[TURF_MAX_PATH_LEN];
  const char* name = cfg->sandbox_name;

  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
  struct oci_state* state = oci_state_load(dest);
  if (state) {
    // TBD: support force flag,
    // stop will send to daemon, and delete sandbox afterwardss
    if (state->state != RLM_STATE_STOPPED) {
      rc = kill(state->pid, 0);

      // is running
      if (rc == 0) {
        error("sandbox is running");
        set_errno(EBUSY);
        rc = -1;
        goto exit;
      }
    }
  }

  // rm dirs
  shl_rmdir2(tfd_path_sandbox(), name);
  shl_rmdir2(tfd_path_overlay(), name);

  rc = 0;

exit:
  if (state) {
    oci_state_free(state);
  }
  return rc;
}

// api for start turf realm
static int tf_do_start(struct tf_cli* cfg) {
  int rc = -1;
  bool success = 0;
  char dest[TURF_MAX_PATH_LEN];
  struct oci_state* state = NULL;
  struct oci_spec* spec = NULL;
  const char* name = cfg->sandbox_name;

  // load spec from sanbox name
  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "config.json");
  spec = oci_spec_load(dest);
  if (!spec) {
    error("sandbox %s not found.", name);
    set_errno(ENOENT);
    goto exit;
  }

  char* runtime = spec->turf.runtime;
  char* code = spec->turf.code;

  if (!runtime) {
    error("runtime is empty.");
    set_errno(ENOENT);
    goto exit;
  }

  if (!code) {
    error("code is empty.");
    set_errno(ENOENT);
    goto exit;
  }

  // exists
  bool is_running = false;
  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
  state = oci_state_load(dest);
  if (state && state->pid &&
      (state->state != RLM_STATE_STOPPED || state->state != RLM_STATE_INIT)) {
    rc = kill(state->pid, 0);
    if (rc == 0) {
      warn("is running");
      is_running = true;
    }
  }

  if (state) {
    oci_state_free(state);
    state = NULL;
  }

  if (is_running) {
    set_errno(EBUSY);
    goto exit;
  }

  // new turf_t
  struct turf_t* tf = tf_new(name);
  struct rlm_t* rlm = tf->realm;

  shl_path3(dest, sizeof(dest), tfd_path_overlay(), name, code);
  rlm_chroot(rlm, dest);

  rlm_arg(rlm, spec->process.arg->argc, spec->process.arg->argv);

  // warm-fork seed process
  if (spec->turf.has.is_seed && spec->turf.is_seed) {
    // makeup socket pair for the seed sandbox
    int sv[2];
    rc = sck_pair_unix_socket(sv);
    rlm_socketpair(rlm, sv);
    dprint("seed %s sv[%d,%d]\n", cfg->sandbox_name, sv[0], sv[1]);

    struct sck_loop* loop = sck_default_loop();
    sck_create_event(loop, sv[0], SCK_READ, tf_seed_on_read, tf);

    // env
    char sz[16];
    snprintf(sz, 16, "%d", sv[1]);
    env_set(spec->process.env, "LD_PRELOAD", tfd_path_libturf());
    env_set(spec->process.env, "TURFPHD_FD", sz);
  }

  // environ
  char** env = spec->process.env->environ;
  rlm_env(rlm, env);

  char* bin = NULL;
  rc = tf_find_binary(&bin, spec->turf.runtime, spec->process.arg->argv[0]);
  if (rc < 0) {
    error("binary not found");
    goto exit;
  }
  rlm->cfg.binary = bin;  // spec->process.argv[0];

  // mem limit
  if (spec->linuxs.resources.has.mem_limit) {
    rlm_limit_mem(rlm, spec->linuxs.resources.mem_limit / 1024);
  }

  // cpu limit
  if (spec->linuxs.resources.has.cpu_quota &&
      spec->linuxs.resources.has.cpu_period &&
      spec->linuxs.resources.cpu_period > 0) {
    uint32_t ms = spec->linuxs.resources.cpu_quota /
                  (spec->linuxs.resources.cpu_period / 1000);
    rlm_limit_cpu(rlm, ms);
  }

  if (spec->process.has.terminal && spec->process.terminal) {
    rlm->cfg.flags.terminal = 1;
  }

  if (cfg->file_stdout) {
    dprint("stdout: %s", cfg->file_stdout);
    rlm->cfg.fd_stdout = strdup(cfg->file_stdout);
    rlm->cfg.flags.fd_stdout = 1;
  }

  if (cfg->file_stderr) {
    dprint("stderr: %s", cfg->file_stderr);
    rlm->cfg.fd_stderr = strdup(cfg->file_stderr);
    rlm->cfg.flags.fd_stderr = 1;
  }

  if (cfg->has.seed) {
    // request fork
    dprint("request %s for clone.", cfg->seed_sandbox_name);

    struct turf_t* stf = tf_find_realm(cfg->seed_sandbox_name);
    if (!stf) {
      error("request seed sandbox `%s` is not found.", cfg->seed_sandbox_name);
      return -1;
    }

    tf_seed_fork_req(stf, tf->realm);

  } else {
    // go sandbox local
    rlm_run(rlm);
  }

  // write oci state
  state = oci_state_create(name, rlm->cfg.chroot_dir);
  if (!state) {
    rlm_kill(rlm, SIGKILL);
    goto exit;
  }

  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");

  // running state
  state->pid = rlm->st.child_pid;

  if (cfg->has.seed) {
    state->state = RLM_STATE_CLONING;  // cloning
  } else {
    state->state = RLM_STATE_RUNNING;  // running
  }

  // TBD: start time
  get_current_time(&state->created);
  oci_state_save(state, dest);

  // waits until sandbox exits
  if (!cfg->has.remote) {  // runc mode
    int exit_code;
    struct rusage ru;
    rlm_wait(rlm, &exit_code, &ru);

    get_current_time(&state->stopped);

    // save stop state
    state->state = RLM_STATE_STOPPED;

    // save exit code
    state->exit_code = exit_code;
    state->has.exit_code = 1;

    // save rusage
    state->rusage = ru;
    state->has.rusage = 1;

    oci_state_save(state, dest);

    tf_free(tf);

  } else {  // C/S mode
    // insert to pid_list
    LIST_INSERT_HEAD(&m_pids, tf, in_list);
  }

  success = 1;
exit:
  if (state) {
    oci_state_free(state);
    state = NULL;
  }
  if (spec) {
    oci_spec_free(spec);
    spec = NULL;
  }
  return success ? 0 : -1;
}

// api for stop(kill -15) turf realm
static int tf_do_stop(struct tf_cli* cfg) {
  int rc = -1;
  char dest[TURF_MAX_PATH_LEN];
  const char* name = cfg->sandbox_name;
  bool success = 0;

  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
  struct oci_state* state = oci_state_load(dest);
  if (!state) {
    error("state for sandbox not found.");
    set_errno(ENOENT);
    goto exit;
  }

  // not running
  if (state->state == RLM_STATE_INIT || state->state == RLM_STATE_STOPPED ||
      state->state == RLM_STATE_KILLING) {
    set_errno(ECHILD);
    goto exit;
  }

  int sig = SIGTERM;
  if (cfg->has.force) {
    sig = SIGKILL;
  }

  // check pid
  if (state->pid <= 1) {
    set_errno(ENOENT);
    goto exit;
  }

  // kill the child
  rc = kill(state->pid, sig);
  if (rc < 0) {
    error("kill failed %d", state->pid);
  }

  int i = 3;
  for (; i >= 0; i--) {
    // let child exits.
    sched_yield();

    rc = kill(state->pid, sig);
    if (rc == -1) {  // not found
      break;
    }
  }

  // reach limits
  if (i < 0) {
    set_errno(EAGAIN);
    goto exit;
  }

  // exited
  struct turf_t* tf = tf_get_realm(state->pid);
  if (tf) {
    LIST_REMOVE(tf, in_list);
    tf_free(tf);
  }

  state->state = RLM_STATE_STOPPED;  // stop
  oci_state_save(state, dest);

  success = 1;

exit:
  if (state) {
    oci_state_free(state);
  }

  return success ? 0 : -1;
}

/* api for get realm state info
 */
static int tf_do_state(struct tf_cli* cfg) {
  int rc = -1;
  char dest[TURF_MAX_PATH_LEN];
  const char* name = cfg->sandbox_name;
  tf_stat st;
  bool success = 0;

  // load spec from sanbox name
  struct oci_state* state = NULL;
  shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
  state = oci_state_load(dest);
  if (!state) {
    error("sandbox %s not found.", name);
    set_errno(ENOENT);
    goto exit;
  }

  int has_pid = 0;
  if (state->pid &&
      (state->state != RLM_STATE_INIT && state->state != RLM_STATE_STOPPED)) {
    rc = pid_stat(state->pid, &st);
    if (rc < 0) {
      state->state = RLM_STATE_STOPPED;
    } else {
      has_pid = 1;
    }
  }

  printf("name: %s\n"
         "pid: %d\n"
         "state: %s\n",
         name,
         state->pid,
         rlm_state_str(state->state));

  if (state->status) {
    if (state->status & RLM_STATUS_CPU_OVL) {
      printf("status.cpu_overload: 1\n");
    }
    if (state->status & RLM_STATUS_MEM_OVL) {
      printf("status.mem_overload: 1\n");
    }
    if (state->status & RLM_STATUS_KILL) {
      printf("status.killed: 1\n");
    }
  } else {
    printf("status: 0\n");
  }

  if (has_pid) {
    printf("stat.utime: %lu\n"
           "stat.stime: %lu\n"
           "stat.cutime: %lu\n"
           "stat.cstime: %lu\n"
           "stat.vsize: %lu\n"
           "stat.rss: %lu\n"
           "stat.minflt: %lu\n"
           "stat.majflt: %lu\n"
           "stat.cminflt: %lu\n"
           "stat.cmajflt: %lu\n"
           "stat.num_threads: %u\n",
           st.utime,
           st.stime,
           st.cutime,
           st.cstime,
           st.vsize,
           st.rss,
           st.min_flt,
           st.maj_flt,
           st.cmin_flt,
           st.cmaj_flt,
           st.num_threads);
  } else {
    // exit_code
    if (state->has.exit_code) {
      // printf("exitcode: %u\n", state->exit_code);
      if (WIFEXITED(state->exit_code)) {
        printf("exitcode: %d\n", WEXITSTATUS(state->exit_code));
      } else if (WIFSIGNALED(state->exit_code)) {
        printf("killed.signal: %d\n", WTERMSIG(state->exit_code));
      }
    }

    if (state->has.rusage) {
      // rusage_dump(&state->rusage);
      printf("rusage.utime: %lu\n"
             "rusage.stime: %lu\n"
             "rusage.masrss: %lu\n",
             timeval2ms(&state->rusage.ru_utime),
             timeval2ms(&state->rusage.ru_stime),
             state->rusage.ru_maxrss);
    }
  }

  success = 1;
exit:
  if (state) {
    oci_state_free(state);
    state = NULL;
  }
  return success ? 0 : -1;
}

// api for list all realm states
static int tf_do_ps(struct tf_cli* cfg) {
  int rc = -1;
  DIR* dir;
  struct dirent* ino;
  dir = opendir(tfd_path_sandbox());
  if (!dir) {
    die("opendir");
  }

  struct pss {
    char* name;
    int pid;
    int state;
  } st;

  char dest[TURF_MAX_PATH_LEN];
  while ((ino = readdir(dir)) != NULL) {
    if ((ino->d_type & DT_DIR) && strcmp(ino->d_name, ".") &&
        strcmp(ino->d_name, "..")) {
      memset(&st, 0, sizeof(st));

      char* name = ino->d_name;
      st.name = name;
      shl_path3(dest, sizeof(dest), tfd_path_sandbox(), name, "state");
      struct oci_state* state = oci_state_load(dest);
      if (state && state->pid) {
        st.pid = state->pid;

        st.state = state->state;
        if (st.state != RLM_STATE_INIT && st.state != RLM_STATE_STOPPED) {
          rc = kill(st.pid, 0);  // check pid
          if (rc < 0) {
            st.state = RLM_STATE_STOPPED;
          }
        }
        oci_state_free(state);
      } else {
        st.pid = 0;
        st.state = RLM_STATE_INIT;
      }
      printf("%-30s %5d %s\n", st.name, st.pid, rlm_state_str(st.state));
    }
  }
  closedir(dir);

  return 0;
}

// create and start
static int tf_do_run(struct tf_cli* cfg) {
  set_errno(ENOTSUP);
  return -1;
}

// show turf information
static int tf_do_info(struct tf_cli* cfg) {
  printf("WORKDIR: %s\n", tfd_path());
  return 0;
}

// turf runtime info
static int tf_do_runtime(struct tf_cli* cfg) {
  DIR* dir;
  struct dirent* ino;
  dir = opendir(tfd_path_runtime());
  if (!dir) {
    error("opendir");
    return -1;
  }

  while ((ino = readdir(dir)) != NULL) {
    if ((ino->d_type & DT_DIR) && strcmp(ino->d_name, ".") &&
        strcmp(ino->d_name, "..")) {
      printf("%-20s \n", ino->d_name);
    }
  }
  closedir(dir);
  return 0;
}

// turf stats info
static int tf_do_events(struct tf_cli* cfg) {
  set_errno(ENOTSUP);
  return -1;
}

// unified entry for turf runc-mode.
int tf_action(struct tf_cli* cfg) {
  int rc;
  dprint("cmd: %d", cfg->cmd);
  switch (cfg->cmd) {
    case TURF_CLI_INIT:
      rc = tf_do_init(cfg);
      break;

    case TURF_CLI_SPEC:
      rc = tf_do_spec(cfg);
      break;

    case TURF_CLI_CREATE:
      rc = tf_do_create(cfg);
      break;

    case TURF_CLI_REMOVE:
      rc = tf_do_delete(cfg);
      break;

    case TURF_CLI_LIST:
      rc = tf_do_list(cfg);
      break;

    case TURF_CLI_STATE:
      rc = tf_do_state(cfg);
      break;

    case TURF_CLI_PS:
      rc = tf_do_ps(cfg);
      break;

    case TURF_CLI_START:
      rc = tf_do_start(cfg);
      break;

    case TURF_CLI_STOP:
      rc = tf_do_stop(cfg);
      break;

    case TURF_CLI_RUN:
      rc = tf_do_run(cfg);
      break;

    case TURF_CLI_INFO:
      rc = tf_do_info(cfg);
      break;

    case TURF_CLI_RUNTIME:
      rc = tf_do_runtime(cfg);
      break;

    case TURF_CLI_EVENTS:
      rc = tf_do_runtime(cfg);
      break;

    default:
      set_errno(ENOSYS);
      rc = -1;
      break;
  }

  return rc;
}
