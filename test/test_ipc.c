#include "bdd-for-c.h"
#include "ipc.h"

spec("turf.ipc") {
  static int rc;
#define MAX_BUF 4096
  static char buf[MAX_BUF];

  it("test.sssize") {
    char* env[] = {"env1=1", "env2=2", "env3=33", 0};
    int s = sssize(env);
    check(s == 3);
  }

  it("ipc.seed_ready") {
    rc = tipc_enc_seed_ready(buf, MAX_BUF);
    check(rc > 0);
    rc = tipc_dec_seed_ready(buf, MAX_BUF);
    check(rc > 0);
  }

  it("ipc.fork") {
    struct rlm_t rlm1;
    rlm1.cfg.name = "sandbox_name";
    rlm1.cfg.binary = "test_binary";
    rlm1.cfg.chroot_dir = "/path/to/chroot";
    rlm1.cfg.fd_stdout = NULL;  //"/path/to/stdout";
    rlm1.cfg.fd_stderr = "/path/to/stderr";
    rlm1.cfg.argc = 3;
    char* argv[] = {"arg1", "arg2", "arg3"};
    char* env[] = {"env1=1", "env2=2", "env3=33", 0};
    rlm1.cfg.argv = argv;
    rlm1.cfg.env = env;
    rlm1.cfg.flags.terminal = 1;
    rc = tipc_enc_fork_req(buf, MAX_BUF, &rlm1);
    printf("msg size == %d\n", rc);
    check(rc > 0);

    struct rlm_t rlm2;
    rc = tipc_dec_fork_req(&rlm2, buf, MAX_BUF);
    check(rc > 0);
    check(rlm1.cfg.flags.terminal == rlm2.cfg.flags.terminal);
    check(strcmp(rlm1.cfg.name, rlm2.cfg.name) == 0);
    check(strcmp(rlm1.cfg.binary, rlm2.cfg.binary) == 0);
    check(strcmp(rlm1.cfg.chroot_dir, rlm2.cfg.chroot_dir) == 0);
    // check(strcmp(rlm1.cfg.fd_stdout, rlm2.cfg.fd_stdout) == 0);
    check(strcmp(rlm1.cfg.fd_stderr, rlm2.cfg.fd_stderr) == 0);
    check(rlm1.cfg.argc == rlm2.cfg.argc);
    for (int i = 0; i < rlm1.cfg.argc; i++) {
      check(strcmp(rlm1.cfg.argv[i], rlm2.cfg.argv[i]) == 0);
    }
    for (int i = 0; i < 3; i++) {
      check(strcmp(rlm1.cfg.env[i], rlm2.cfg.env[i]) == 0);
    }
  }

  it("ipc.short") {
    struct rlm_t rlm1 = {0};
    rlm1.cfg.name = "sandbox_name";
    rlm1.cfg.flags.terminal = 1;
    rc = tipc_enc_fork_req(buf, 30, &rlm1);
    check(rc == -1);
    check(errno == EMSGSIZE);
  }

  it("ipc.clone") {
    rc = 1;
    printf("%p, %d\n", &rc, rc);
  }

  it("ipc.clone2") {
    printf("%p, %d\n", &rc, rc);
  }
}
