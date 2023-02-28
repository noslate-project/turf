#include "realm.h"

/* see man capabilities(7) and man cap_get_proc(3) for detail.
 */

#if 0
static int sysadmin_makeup_rootfs()
{

    return 0; 
}

static uint32_t get_cap_last_cap()
{
	uint32_t last_valid_cap = 0;
    // get capb from kernel
    FILE *fp = fopen("/proc/sys/kernel/cap_last_cap", "re");
    if (fscanf(fp, "%u", &last_valid_cap) != 1) {
        return -1;
    }
    fclose(fp);
	return last_valid_cap;
}

void drop_capbset(uint64_t keep_mask, uint32_t last_valid_cap)
{
	const uint64_t one = 1;
	uint64_t i;
	for (i = 0; i < sizeof(keep_mask) * 8 && i <= last_valid_cap; ++i) {
		if (keep_mask & (one << i))
			continue;
		if (prctl(PR_CAPBSET_DROP, i))
			die("could not drop capability from bounding set");
	}
}
#endif

/* closed by loy
 * TBD: static link libcap & libattr.
 */
#if 0 
void drop_caps(const struct tf_t *t, uint32_t last_valid_cap)
{
	cap_t caps = cap_get_proc();
	cap_value_t flag[1];
	const uint64_t one = 1;
	unsigned int i;
	
    if (!caps) {
		die("can't get process caps");
    }

	if (cap_clear_flag(caps, CAP_INHERITABLE))
		die("can't clear inheritable caps");
	if (cap_clear_flag(caps, CAP_EFFECTIVE))
		die("can't clear effective caps");
	if (cap_clear_flag(caps, CAP_PERMITTED))
		die("can't clear permitted caps");

	for (i = 0; i < sizeof(t->cfg.capbset_mask) * 8 && i <= last_valid_cap; ++i) {
		/* Keep CAP_SETPCAP for dropping bounding set bits. */
		if (i != CAP_SETPCAP && !(t->cfg.capbset_mask & (one << i)))
			continue;
		flag[0] = i;
		if (cap_set_flag(caps, CAP_EFFECTIVE, 1, flag, CAP_SET))
			die("can't add effective cap");
		if (cap_set_flag(caps, CAP_PERMITTED, 1, flag, CAP_SET))
			die("can't add permitted cap");
		if (cap_set_flag(caps, CAP_INHERITABLE, 1, flag, CAP_SET))
			die("can't add inheritable cap");
	}
	if (cap_set_proc(caps))
		die("can't apply initial cleaned capset");

	drop_capbset(t->cfg.capbset_mask, last_valid_cap);

	if ((t->cfg.capbset_mask & (one << CAP_SETPCAP)) == 0) {
		flag[0] = CAP_SETPCAP;
		if (cap_set_flag(caps, CAP_EFFECTIVE, 1, flag, CAP_CLEAR))
			die("can't clear effective cap");
		if (cap_set_flag(caps, CAP_PERMITTED, 1, flag, CAP_CLEAR))
			die("can't clear permitted cap");
		if (cap_set_flag(caps, CAP_INHERITABLE, 1, flag, CAP_CLEAR))
			die("can't clear inheritable cap");
	}

	if (cap_set_proc(caps))
		die("can't apply final cleaned capset");

	cap_free(caps);
}
#endif

/* see man cgroups(7) for detail.
 */

int cg_memory_limit(struct rlm_t* r) {
  return 0;
}

int cg_cpu_limit(struct rlm_t* re) {
  return 0;
}

// enter pivot root
int sys_pivot_root(struct rlm_t* r) {
  int oldroot, newroot;

  oldroot = open("/", O_DIRECTORY | O_RDONLY | O_CLOEXEC);
  if (oldroot < 0) {
    die("failed to open / for fchdir");
  }
  newroot = open(r->cfg.chroot_dir, O_DIRECTORY | O_RDONLY | O_CLOEXEC);
  if (newroot < 0) {
    die("failed to open %s for fchdir", r->cfg.chroot_dir);
  }

  // pivot the root of a filesystem
  if (mount(
          r->cfg.chroot_dir, r->cfg.chroot_dir, "bind", MS_BIND | MS_REC, "")) {
    die("failed to bind mount '%s'", r->cfg.chroot_dir);
  }
  if (chdir(r->cfg.chroot_dir)) {
    return -errno;
  }
  if (pivot_root(".", ".")) {
    die("pivot_root");
  }

  // umount the old root
  if (fchdir(oldroot)) {
    die("failed to fchdir to old /");
  }
  if (mount(NULL, ".", NULL, MS_REC | MS_PRIVATE, NULL)) {
    die("failed to mount(/, private) before umount(/)");
  }
  if (umount2(".", MNT_DETACH)) {
    die("umount(/)");
  }

  // Change back to the new root.
  if (fchdir(newroot)) {
    return -errno;
  }
  if (close(oldroot)) {
    return -errno;
  }
  if (close(newroot)) {
    return -errno;
  }
  if (chroot("/")) {
    return -errno;
  }

  // change to new '/'.
  if (chdir("/")) {
    return -errno;
  }

  return 0;
}

int _API sysadmin_enter(struct rlm_t* t) {
  return 0;
}

int _API sysadmin_run(struct rlm_t* t) {
  pid_t child_pid;

  // leader the process group.
  if (setpgid(0, 0) != 0) {
    die("setpgid");
  }

  child_pid = fork();
  if (child_pid) {
    /*
     * go parent.
     */

    if (child_pid < 0) {
      die("fork()");
    }

    printf("child at %d\n", child_pid);
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
  printf("process for child.\n");

  // close parent stdxxx fds
  int i;
  for (i = 0; i <= 2; i++) {
    if (dup2(i, i) < 0) {
      die("dup2");
    }
  }

#if 0 
    if (setresuid(0,0,0)!=0) {
        die("setresuid");
    }

    if (setresgid(0,0,0)!=0) {
        die("setresgid");
    }
#endif

  if (unshare(CLONE_NEWNS)) {
    die("unshare(NEWNS)");
  }

#if 0  // kernel > 3.8
    if (unshare(CLONE_NEWCGROUP)) {
        die("unshare(NEWCGROUP)");
    }
#endif

  // seprate from parent group.
  if (setsid() < 0) {
    die("setsid failed.");
  }

  if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0)) {
    die("no_new_privs");
  }

  if (t->cfg.flags.chroot && sys_pivot_root(t)) {
    die("chroot");
  }

  // execve node
  execve(t->cfg.argv[0], t->cfg.argv, environ);

  return 0;
}
