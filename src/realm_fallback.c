#include "realm.h"

static int fallback_enter(struct rlm_t* t) {
  return 0;
}

static int close_fds(int* inheritable_fds, size_t size) {
  const char* kFdPath = "/proc/self/fd";

  DIR* d = opendir(kFdPath);
  struct dirent* dir_entry;

  if (d == NULL) return -1;
  int dir_fd = dirfd(d);
  while ((dir_entry = readdir(d)) != NULL) {
    size_t i;
    char* end;
    bool should_close = true;
    const int fd = strtol(dir_entry->d_name, &end, 10);

    if ((*end) != '\0') {
      continue;
    }
    /*
     * We might have set up some pipes that we want to share with
     * the parent process, and should not be closed.
     */
    for (i = 0; i < size; ++i) {
      if (fd == inheritable_fds[i]) {
        should_close = false;
        break;
      }
    }
    /* Also avoid closing the directory fd. */
    if (should_close && fd != dir_fd) close(fd);
  }
  closedir(d);
  return 0;
}

static int fallback_run(struct rlm_t* t) {
  pid_t child_pid;

  // TBD: leader the process group during init
  // leader the process group.
  if (setpgid(0, 0) != 0) {
    die("setpgid");
  }

  /* execve afterwards, vfork()
   */
#if defined(__linux__)
  int clone_flag = SIGCHLD;
  child_pid = syscall(__NR_clone, clone_flag, NULL);
#else
  child_pid = vfork();
#endif

  if (child_pid) {
    /*
     * go parent.
     */

    if (child_pid < 0) {
      die("fork()");
    }

    info("child at %d", child_pid);
    t->st.child_pid = child_pid;

    // TBD: cgroup cfg
#if 0
        char buf[32];
        int rc = snprintf(buf, 32, "%u", child_pid);
        rc = write_proc("/sys/fs/cgroup/memory/turf/app-1/cgroup.procs", buf, rc); 
        rc = write_proc("/sys/fs/cgroup/cpu/turf/app-1/cgroup.procs", buf, rc);
#endif
    // TBD: namespace cfg

    return 0;
  }

  /*
   * go child process
   */
  info("process for child.");

#if defined(__linux__)
  // want child be killed if parent's gone.
  prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif

  if (t->cfg.flags.terminal) {
    int i;
    for (i = 0; i <= 2; i++) {
      if (dup2(i, i) < 0) {
        die("dup2");
      }
    }
  } else {
    int fd = -1, fout = -1, ferr = -1;

    fd = open("/dev/null", O_RDWR);

    if (t->cfg.flags.fd_stdout && t->cfg.fd_stdout) {
      fout = open(t->cfg.fd_stdout, O_WRONLY | O_APPEND | O_CREAT, 00644);
      if (fout < 0) {
        error("open stdout failed");
      }
    }
    if (t->cfg.flags.fd_stderr && t->cfg.fd_stderr) {
      ferr = open(t->cfg.fd_stderr, O_WRONLY | O_APPEND | O_CREAT, 00644);
      if (ferr < 0) {
        error("open stderr failed");
      }
    }

    // close stdin
    dup2(fd, 0);

    // redirect stdout
    if (fout > 0) {
      dup2(fout, 1);
      close(fout);
    } else {
      dup2(fd, 1);
    }

    // redirect stderr
    if (ferr > 0) {
      dup2(ferr, 2);
      close(ferr);
    } else {
      dup2(fd, 2);
    }

    // close fd
    close(fd);
  }

  // close fds
  {
    int keep_fds[128];  // reserved max
    int k = 0;

    // stdin, out, err
    keep_fds[k++] = 0;
    keep_fds[k++] = 1;
    keep_fds[k++] = 2;

    if (t->cfg.flags.socketpair) {
      // only client sv keeped in child.
      // keep_fds[k++] = t->cfg.sv[0];
      keep_fds[k++] = t->cfg.sv[1];
    }

    close_fds(keep_fds, k);
  }

#if 0  // setuid needs CAP_SYS_ADMIN 
    if (setresuid(0,0,0)!=0) {
        die("setresuid");
    }

    if (setresgid(0,0,0)!=0) {
        die("setresgid");
    }
#endif

#if 0  // unshare needs CAP_SYS_ADMIN
    if (unshare(CLONE_NEWNS)) {
        die("unshare(NEWNS)");
    }
#endif

#if 0  // kernel > 3.8
    if (unshare(CLONE_NEWCGROUP)) {
        die("unshare(NEWCGROUP)");
    }
#endif

  // seprate from parent group.
  if (setsid() < 0) {
    perror("setsid failed.");
  }

#if defined(__linux__)
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    die("no_new_privs");
  }
#endif
#if 1  // chroot needs CAP_CHROOT
  if (t->cfg.flags.chroot) {
    chdir(t->cfg.chroot_dir);
    dprint("chdir %s", t->cfg.chroot_dir);
  }
#endif
  info("now go sandbox");
  execve(t->cfg.binary, t->cfg.argv, t->cfg.env);

  // should not return
  return 0;
}

static int fallback_fork(struct rlm_t* t) {
  pid_t child_pid;

  // TBD: leader the process group during init
  // leader the process group.
  if (setpgid(0, 0) != 0) {
    warn("setpgid");
  }

  /* execve afterwards, vfork()
   */
#if defined(__linux__)
  int clone_flag = SIGCHLD | CLONE_PARENT;
  child_pid = syscall(__NR_clone, clone_flag, NULL);
#else
  child_pid = fork();
#endif

  if (child_pid) {
    /*
     * go parent.
     */

    if (child_pid < 0) {
      die("fork()");
    }

    info("child at %d", child_pid);
    t->st.child_pid = child_pid;

    // TBD: cgroup cfg
#if 0
        char buf[32];
        int rc = snprintf(buf, 32, "%u", child_pid);
        rc = write_proc("/sys/fs/cgroup/memory/turf/app-1/cgroup.procs", buf, rc); 
        rc = write_proc("/sys/fs/cgroup/cpu/turf/app-1/cgroup.procs", buf, rc);
#endif
    // TBD: namespace cfg

    return child_pid;
  }

  /*
   * go child process
   */
  info("process for child.");

#if defined(__linux__)
  // want child be killed if parent's gone.
  prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif

  if (t->cfg.flags.terminal) {
    int i;
    for (i = 0; i <= 2; i++) {
      if (dup2(i, i) < 0) {
        die("dup2");
      }
    }
  } else {
    int fd = -1, fout = -1, ferr = -1;

    fd = open("/dev/null", O_RDWR);

    if (t->cfg.flags.fd_stdout && t->cfg.fd_stdout) {
      fout = open(t->cfg.fd_stdout, O_WRONLY | O_CREAT | O_APPEND, 00644);
      if (fout < 0) {
        error("open stdout failed");
      }
    }
    if (t->cfg.flags.fd_stderr && t->cfg.fd_stderr) {
      ferr = open(t->cfg.fd_stderr, O_WRONLY | O_CREAT | O_APPEND, 00644);
      if (ferr < 0) {
        error("open stderr failed");
      }
    }

    // close stdin
    dup2(fd, 0);

    // redirect stdout
    if (fout > 0) {
      dup2(fout, 1);
      close(fout);
    } else {
      dup2(fd, 1);
    }

    // redirect stderr
    if (ferr > 0) {
      dup2(ferr, 2);
      close(ferr);
    } else {
      dup2(fd, 2);
    }

    // close fd
    close(fd);
  }

#if 0
    // close fds
    {
        int keep_fds[128]; // reserved max 
        int k = 0;

        // stdin, out, err 
        keep_fds[k++] = 0;
        keep_fds[k++] = 1;
        keep_fds[k++] = 2;

        close_fds(keep_fds, k);
    }
#endif

#if 0  // setuid needs CAP_SYS_ADMIN 
    if (setresuid(0,0,0)!=0) {
        die("setresuid");
    }

    if (setresgid(0,0,0)!=0) {
        die("setresgid");
    }
#endif

#if 0  // unshare needs CAP_SYS_ADMIN
    if (unshare(CLONE_NEWNS)) {
        die("unshare(NEWNS)");
    }
#endif

#if 0  // kernel > 3.8
    if (unshare(CLONE_NEWCGROUP)) {
        die("unshare(NEWCGROUP)");
    }
#endif

  // seprate from parent group.
  if (setsid() < 0) {
    perror("setsid failed.");
  }

#if defined(__linux__)
  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    die("no_new_privs");
  }
#endif
#if 1  // chroot needs CAP_CHROOT
  if (t->cfg.flags.chroot) {
    chdir(t->cfg.chroot_dir);
    dprint("chdir %s", t->cfg.chroot_dir);
  }
#endif

#ifdef __linux__
  clearenv();
#endif
  for (int i = 0; i < sssize(t->cfg.env); i++) {
    putenv(t->cfg.env[i]);
  }

  info("now go sandbox");
  // execve(t->cfg.binary, t->cfg.argv, t->cfg.env);

  // should not return
  return child_pid;
}
