#include "bdd-for-c.h"
#include "cli.h"
#include "shell.h"

static int cmd2args(const char* str, int* argc, char** argv[]) {
  char buf[TURF_MAX_CMD_LEN];
  char* pp[32];
  int i = 0;

  strncpy(buf, str, TURF_MAX_CMD_LEN);
  buf[TURF_MAX_CMD_LEN - 1] = 0;

  char* p = strtok(buf, " ");
  while (p) {
    pp[i++] = strdup(p);
    p = strtok(NULL, " ");
  }
  *argc = i;
  *argv = (char**)calloc(i + 1, sizeof(char*));
  memcpy(*argv, pp, i * (sizeof(char*)));
  return 0;
}

spec("turf.cli") {
  int rc;
  it("cli.aoti") {
    uint32_t v = 0;
    rc = cli_get_uint32(&v, "1", 0);
    check(rc == 0);
    check(v == 1);

    rc = cli_get_uint32(&v, "-1", 3);
    check(rc != 0);
    check(v == 3);

    rc = cli_get_uint32(&v, "abc", 33);
    check(rc == 0);
    check(v == 0);

    uint64_t k = 0;
    rc = cli_get_uint64(&k, "1", 0);
    check(rc == 0);
    check(k == 1);

    rc = cli_get_uint64(&k, "-1", 3);
    check(rc != 0);
    check(k == 3);

    rc = cli_get_uint64(&k, "abc", 33);
    check(rc == 0);
    check(k == 0);
  }

  static const char* cmd;
  static int c;
  static char** v;
  static tf_cli mcli;
  static tf_cli* cli = &mcli;
  it("cli.init") {
    cmd = "turf init";
    rc = cmd2args(cmd, &c, &v);
    check(c == 2);
    check(rc == 0);
    check(strcmp(v[0], "turf") == 0);
    check(strcmp(v[1], "init") == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_INIT);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.spec") {
    cmd = "turf spec -c 10 -m 32";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_SPEC);
    check((cli->cpulimit == 10) && (cli->has.cpulimit));
    check((cli->memlimit == 32) && (cli->has.memlimit));

    cli_free(cli);
    ssfree(v);
  }

  it("cli.spec2") {
    cmd = "turf spec --cpu 23 --mem 50";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_SPEC);
    check((cli->cpulimit == 23) && (cli->has.cpulimit));
    check((cli->memlimit == 50) && (cli->has.memlimit));

    cli_free(cli);
    ssfree(v);
  }

  it("cli.create") {
    cmd = "turf create -b /path/to/bundle sandbox_name1";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_CREATE);
    check(strcmp(cli->bundle_path, "/path/to/bundle") == 0);
    check(strcmp(cli->sandbox_name, "sandbox_name1") == 0);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.delete") {
    cmd = "turf delete sandbox_name2";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_REMOVE);
    check(strcmp(cli->sandbox_name, "sandbox_name2") == 0);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.start") {
    cmd = "turf start --stdout /path_to/stdout.log --stderr "
          "/path_to/stderr.log sandbox_name3";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_START);
    check(strcmp(cli->sandbox_name, "sandbox_name3") == 0);
    check(strcmp(cli->file_stdout, "/path_to/stdout.log") == 0);
    check(strcmp(cli->file_stderr, "/path_to/stderr.log") == 0);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.stop") {
    cmd = "turf stop sandbox_name4";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_STOP);
    check(strcmp(cli->sandbox_name, "sandbox_name4") == 0);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.state") {
    cmd = "turf state sandbox_name5";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_STATE);
    check(strcmp(cli->sandbox_name, "sandbox_name5") == 0);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.list") {
    cmd = "turf list";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_LIST);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.ps") {
    cmd = "turf ps";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_PS);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.run") {
    cmd = "turf run";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_RUN);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.info") {
    cmd = "turf info";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_INFO);

    cli_free(cli);
    ssfree(v);
  }

  it("cli.runtime") {
    cmd = "turf runtime";
    rc = cmd2args(cmd, &c, &v);
    check(rc == 0);

    rc = cli_parse(cli, c, v);
    check(rc == 0);
    check(cli->cmd == TURF_CLI_RUNTIME);

    cli_free(cli);
    ssfree(v);
  }
}
