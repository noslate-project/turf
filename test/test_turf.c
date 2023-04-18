#include "bdd-for-c.h"
#include "shell.h"
#include "turf.h"

spec("turf") {
  it("turf.create") {
    int rc;
    struct tf_cli cfg = {0};
    char dest[1024];

    shl_path3(dest, 1024, getenv("TURF_WORKDIR"), "/bundle", "pi");
    puts(dest);

    chdir(dest);
    cfg.sandbox_name = "utest-pi";
    // clean previous
    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);

    cfg.cmd = TURF_CLI_SPEC;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_CREATE;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_LIST;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_START;
    rc = tf_action(&cfg);
    check(rc == 0);

    // stop
    cfg.cmd = TURF_CLI_STOP;
    rc = tf_action(&cfg);

    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);
    check(rc == 0);
  }

  it("turf.run") {
    int rc;
    struct tf_cli cfg = {0};
    char dest[1024];

    shl_path3(dest, 1024, getenv("TURF_WORKDIR"), "/bundle", "pi");
    puts(dest);

    chdir(dest);
    cfg.sandbox_name = "utest-pi";
    // clean previous
    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);

    cfg.cmd = TURF_CLI_SPEC;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_RUN;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_LIST;
    rc = tf_action(&cfg);
    check(rc == 0);

    // stop
    cfg.cmd = TURF_CLI_STOP;
    rc = tf_action(&cfg);

    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);
    check(rc == 0);
  }

  it("turf.start executable not found") {
    int rc;
    struct tf_cli cfg = {0};
    char dest[1024];

    shl_path3(dest, 1024, getenv("TURF_WORKDIR"), "/bundle", "no-executable");
    puts(dest);

    chdir(dest);
    cfg.sandbox_name = "utest-no-executable";
    // clean previous
    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);

    cfg.cmd = TURF_CLI_CREATE;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_LIST;
    rc = tf_action(&cfg);
    check(rc == 0);

    cfg.cmd = TURF_CLI_START;
    rc = tf_action(&cfg);
    check(rc == -1);

    // state:
    cfg.cmd = TURF_CLI_STATE;
    rc = tf_action(&cfg);
    check(rc == -1);

    // stop
    cfg.cmd = TURF_CLI_STOP;
    rc = tf_action(&cfg);
    check(rc == -1);

    cfg.cmd = TURF_CLI_REMOVE;
    rc = tf_action(&cfg);
    check(rc == 0);
  }
}
