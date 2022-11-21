/* cli frontend of turf
 */

#include "cli.h"
#include "daemon.h"

void _API cli_show_version() {
#define TURF_V_MAJOR 0
#define TURF_V_MINOR 0
#define TURF_V_PATCH 6

#ifndef GIT_VERSION
#define GIT_VERSION "dev"
#endif

#define TURF_VERSION_STRINGFY(a, b, c, d)                                      \
  TURF_VERSION_STRINGFY_(a)                                                    \
  "." TURF_VERSION_STRINGFY_(b) "." TURF_VERSION_STRINGFY_(c) "-" d
#define TURF_VERSION_STRINGFY_(a) #a

#define TURF_VERSION                                                           \
  TURF_VERSION_STRINGFY(TURF_V_MAJOR, TURF_V_MINOR, TURF_V_PATCH, GIT_VERSION)

  printf("Alinode turf, version %s\n", TURF_VERSION);
  printf("Build time %s, %s\n", __TIME__, __DATE__);
  exit(0);
}

void _API cli_show_help(void) {
  const char* help =
      "\n"
      "Usage: turf [MODE] COMMAND [OPTIONS]\n"
      "\n"
      "A light-weight sandbox for noslate project.\n"
      "\n"
      "MODE:\n"
      "  -D, --daemon         C/S mode daemon\n"
      "  -H, --host           C/S mode client, host to connect to\n"
      "  -v, --version        Print version information and quit\n"
      "  -h, --help           show help and quit\n"
      "\n"
      "Commands:\n"
      "  create               Create a new sandbox\n"
      "  delete               Delete a sandbox\n"
      "  events               Show events such as OOM, cpu, memory and IO "
      "usage statistic\n"
      "  info                 Show turf information\n"
      "  list                 List all sandbox\n"
      "  ps                   List sandbox with resource usage statistic\n"
      "  runtime              Manage runtime\n"
      "  spec                 Create a new specification file\n"
      "  start                Start a stopped sandbox\n"
      "  state                Show state of a sandbox\n"
      "  stats                Show resource usage statistic\n"
      "  stop                 Stop a running sandbox\n"
      "\n"
      "Run 'turf -D --help' for more information on C/S daemon.\n"
      "Run 'turf -H --help' for more information on C/S client.\n"
      "Run 'turf COMMAND --help' for more information on a command.\n";
  puts(help);
}

// get uint32 value, or def on error
int cli_get_uint32(uint32_t* v, const char* str, uint32_t def) {
  int i = atoi(str);
  if (i < 0) {
    *v = def;
    set_errno(EINVAL);
    return -1;
  }
  *v = i;
  return 0;
}

int cli_get_uint64(uint64_t* v, const char* str, uint64_t def) {
  int64_t i = atoll(str);
  if (i < 0) {
    *v = def;
    set_errno(EINVAL);
    return -1;
  }
  *v = i;
  return 0;
}

// 0: good name
static int cli_chk_sandbox_name(const char* str) {
  if ((strlen(str) < TURF_SBX_MIN_LEN) || (strlen(str) > TURF_SBX_MAX_LEN)) {
    set_errno(EINVAL);
    return -1;
  }

  if (strchr(str, '/') || strchr(str, '\\')) {
    set_errno(EINVAL);
    return -1;
  }

  return 0;
}

// init turf workspace
static int cli_init(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+h";
  const struct option lo[] = {{"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf init\n"
      "\n"
      "Initialize the turf workdir\n"
      "\n"
      "The command initialize the turf_workdir for sandbox and runtime "
      "storage,\n"
      "by 'TURF_WORKDIR' environ variable specified or '~/.turf' by default.\n"
      "\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      default:
        set_errno(EINVAL);
        return -1;
    }
  }

  cli->cmd = TURF_CLI_INIT;
  return 0;
}

static int cli_info(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+h";
  const struct option lo[] = {{"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf info\n"
      "\n"
      "Display turf information\n"
      "\n"
      "The command shows turf setting, couters, performance etc...\n"
      "\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      default:
        set_errno(EINVAL);
        return -1;
    }
  }

  cli->cmd = TURF_CLI_INFO;
  return 0;
}

static int cli_spec(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+hb:c:m:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"bundle", required_argument, 0, 'b'},
                              {"mem", required_argument, 0, 'm'},
                              {"cpu", required_argument, 0, 'c'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf spec [OPTIONS]\n"
      "\n"
      "Create a new turf specification file\n"
      "\n"
      "The command creates a new specification file named 'config.json' for "
      "the turf\n"
      "bundle dir. The spec file generated by turf is a starter file, please "
      "editing\n"
      "it to match your desired results.\n"
      "\n"
      "Options:\n"
      "  -b, --bundle path        path to root of ohe bundle dir\n"
      "  -c, --cpu int            Limit CPU in percent\n"
      "  -m, --mem int            Limit Memory in MBytes\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      case 'b':  // bundle
        cli->bundle_path = strdup(optarg);
        break;

      case 'm':  // memlimit
        cli_get_uint32(&cli->memlimit, optarg, 0);
        cli->has.memlimit = 1;
        break;

      case 'c':  // cpu
        cli_get_uint32(&cli->cpulimit, optarg, 0);
        cli->has.cpulimit = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  cli->cmd = TURF_CLI_SPEC;
  return 0;
}

static int cli_create(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+hb:m:c:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"bundle", required_argument, 0, 'b'},
                              {"mem", required_argument, 0, 'm'},
                              {"cpu", required_argument, 0, 'c'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf create [OPTIONS] SANDBOX_NAME\n"
      "\n"
      "Create a new sandbox\n"
      "\n"
      "The command creates a new named sandbox for the bundle, the bundle is a "
      "directory\n"
      "with a specification file named 'config.json' and a code directory, the "
      "spec file\n"
      "inclues the running parameter, the code dir could be aworker\n"
      "codebase. The name of a sandbox must be unique in turf system-wide.\n"
      "\n"
      "Options:\n"
      "  -b, --bundle path        path to the root of the bundle dir\n"
      "  -c, --cpu int            override limit CPU in percent\n"
      "  -m, --mem int            override limit Memory in MBytes\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);

      case 'b':  // bundle
        cli->bundle_path = strdup(optarg);
        break;

      case 'c':  // override cpulimit
        cli_get_uint32(&cli->cpulimit, optarg, 0);
        cli->has.cpulimit = 1;
        break;

      case 'm':  // override memlimit
        cli_get_uint32(&cli->memlimit, optarg, 0);
        cli->has.memlimit = 1;
        break;

      default:
        error("unknown cmd %d", opt);
        set_errno(EINVAL);
        return -1;
    };
  }

  if (argc == optind) {
    error("no sandbox name");
    set_errno(EINVAL);
    return -1;
  }

  const char* name = argv[optind];
  if (cli_chk_sandbox_name(name)) {  // sandbox minium length
    error("illegal sandbox name.");
    set_errno(EINVAL);
    return -1;
  }
  cli->sandbox_name = strdup(name);

  cli->cmd = TURF_CLI_CREATE;
  return 0;
}

// delete a turf sandbox
static int cli_delete(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+hf";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"force", no_argument, 0, 'f'},
                              {0, 0, 0, 0}};

  const char* help = "\n"
                     "Usage : turf delete [OPTIONS] SANDBOX_NAME\n"
                     "\n"
                     "Delete a sandbox\n"
                     "\n"
                     "The command deletes the name and resources held by the "
                     "sandbox, often used\n"
                     "with stopped sandbox."
                     "\n"
                     "Options:\n"
                     "  -f, --force      Force delete the sandbox if it is "
                     "still running (SIGKILL)\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);

      case 'f':  // bundle
        cli->has.force = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  if (argc == optind) {
    error("no sandbox name");
    set_errno(EINVAL);
    return -1;
  }

  const char* name = argv[optind];
  if (cli_chk_sandbox_name(name)) {  // sandbox minium length
    error("illegal sandbox name");
    set_errno(EINVAL);
    return -1;
  }
  cli->sandbox_name = strdup(name);

  cli->cmd = TURF_CLI_REMOVE;
  return 0;
}

static int cli_start(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+he:s:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"stdout", required_argument, 0, 2},
                              {"stderr", required_argument, 0, 3},
                              {"env", required_argument, 0, 'e'},
                              {"seed", required_argument, 0, 's'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf start [OPTIONS] SANDBOX_NAME\n"
      "\n"
      "Bring up a sandbox to run\n"
      "\n"
      "The command executes the user defined application in a created "
      "sandbox.\n"
      "\n"
      "Options:\n"
      "      --stdout path    Redirect stdout of the sandbox to path\n"
      "      --stderr path    Redirect stderr of the sandbox to path\n"
      "  -e, --env K=V        Override envrion variables in specification "
      "file\n"
      "  -s, --seed sandbox   Warm fork seed sandbox\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);

      case 2:  // stdout
        cli->file_stdout = strdup(optarg);
        break;

      case 3:  // stderr
        cli->file_stderr = strdup(optarg);
        break;

      case 'e':  // environ
        if (cli->env_cnt < TURF_CLI_MAX_ENV) {
          cli->envs[cli->env_cnt++] = strdup(optarg);
        }
        break;

      case 'f':  // TBD: override setting.
        cli->has.force = 1;
        break;

      case 's':  // warmfork
        cli->seed_sandbox_name = strdup(optarg);
        cli->has.seed = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  if (argc == optind) {
    error("no sandbox name");
    set_errno(EINVAL);
    return -1;
  }

  const char* name = argv[optind];
  if (cli_chk_sandbox_name(name)) {
    error("illegal sandbox name");
    set_errno(EINVAL);
    return -1;
  }
  cli->sandbox_name = strdup(name);

  cli->cmd = TURF_CLI_START;
  return 0;
}

static int cli_stop(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+k:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"time", no_argument, 0, 't'},
                              {"force", no_argument, 0, 'f'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf stop [OPTIONS] SANDBOX_NAME\n"
      "\n"
      "Stop a running sandbox\n"
      "\n"
      "The command uses SIGTERM to terminate a sandbox and wait it exited.\n"
      "\n"
      "Options:\n"
      "  -f, --force          Force stop a running sandbox (uses SIGKILL)\n"
      "  -t, --time int       Seconds to wait for stop before killing it "
      "(default 3)\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);

      case 't':  // time wait
        cli_get_uint32(&cli->time_wait, optarg, TURF_CLI_DEFAULT_TIME_WAIT);
        cli->has.time_wait = 1;
        break;

      case 'f':  // force kill ( kill -9 )
        cli->has.force = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  if (argc == optind) {
    error("no sandbox name");
    set_errno(EINVAL);
    return -1;
  }

  const char* name = argv[optind];
  if (cli_chk_sandbox_name(name)) {
    error("illegal sandbox name");
    set_errno(EINVAL);
    return -1;
  }
  cli->sandbox_name = strdup(name);

  cli->cmd = TURF_CLI_STOP;
  return 0;
}

static int cli_list(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+hf:v";
  const struct option lo[] = {{"help", no_argument, 0, 'h'}, {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf list [OPTIONS]\n"
      "\n"
      "List all sandbox the turf created\n"
      "\n"
      "Options:\n"
      "  -f, --format value   Output in table or json (default: table)\n"
      "  -v, --verbose        Output more details instead only names\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      default:
        set_errno(EINVAL);
        return -1;
    }
  }

  cli->cmd = TURF_CLI_LIST;
  return 0;
}

static int cli_state(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+f:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"format", no_argument, 0, 'f'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf state [OPTIONS] SANDBOX_NAME\n"
      "\n"
      "Display the running state of the sandbox\n"
      "\n"
      "Options:\n"
      "  -f, --format value   Output in table or json (default: table)\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      case 'f':  // output in json
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  if (argc == optind) {
    error("no sandbox name");
    set_errno(EINVAL);
    return -1;
  }

  const char* name = argv[optind];
  if (cli_chk_sandbox_name(name)) {  // sandbox minium length
    error("illegal sandbox name");
    set_errno(EINVAL);
    return -1;
  }
  cli->sandbox_name = strdup(name);

  cli->cmd = TURF_CLI_STATE;
  return 0;
}

static int cli_ps(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+f:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"format", no_argument, 0, 'f'},
                              {0, 0, 0, 0}};

  const char* help =
      "\n"
      "Usage : turf ps [OPTIONS]\n"
      "\n"
      "Display all running sandbox processes info\n"
      "\n"
      "Options:\n"
      "  -f, --format value   Output in table or json (default: table)\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      case 'f':  // output in json
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  cli->cmd = TURF_CLI_PS;
  return 0;
}

static int cli_run(tf_cli* cli, int argc, char* const argv[]) {
  cli->cmd = TURF_CLI_RUN;
  return 0;
}

static int cli_runtime(tf_cli* cli, int argc, char* const argv[]) {
  int rc = 0;
  const char* so = "+i:";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"install", required_argument, 0, 'i'},
                              {0, 0, 0, 0}};

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'i':
        cli->runtime_name = strdup(optarg);
        cli->has.install = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    }
  }

  cli->cmd = TURF_CLI_RUNTIME;
  return rc;
}

static int cli_daemon(tf_cli* cli, int argc, char* const argv[]) {
  const char* so = "+D::fh";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"daemon", optional_argument, 0, 'D'},
                              {"host", optional_argument, 0, 'H'},
                              {"foreground", no_argument, 0, 'f'},
                              {0, 0, 0, 0}};

  const char* help = "\n"
                     "Usage : turf -D [OPTIONS]\n"
                     "\n"
                     "Daemon process for managing all turf sandbox (turfd).\n"
                     "\n"
                     "Turf supports runc mode and C/S mode whole in one "
                     "binary, the 'turf -D' command\n"
                     "acts the turf process as a daemon process, all sandbox "
                     "processes are children of\n"
                     "the daemon, the deamon manages all the running sandbox "
                     "and the resources held.\n"
                     "The 'turf -H' acts as a client to communicate with the "
                     "daemon, when any command\n"
                     "supports C/S mode, you can use like 'turf -H start' to "
                     "invoke remotely instead\n"
                     "of 'turf start' in runc mode for example.\n"
                     "\n"
                     "Options:\n"
                     "  -f, --foregound      Daemon runs foreground\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      case 'D':  // daemon
        cli->has.remote = 1;
        cli->cmd = TURF_CLI_DAEMON;
        break;

      case 'f':  // foreground
        cli->has.foreground = 1;
        break;

      case 'H':  // host
        cli->has.remote = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }
  return 0;
}

static int cli_remote(tf_cli* cli, int argc, char* const argv[]) {
  int rc = 0;
  const char* so = "+H::h";
  const struct option lo[] = {{"help", no_argument, 0, 'h'},
                              {"host", optional_argument, 0, 'H'},
                              {0, 0, 0, 0}};

  const char* help = "\n"
                     "Usage : turf -H [OPTIONS] COMMANDS\n"
                     "\n"
                     "Client for communicating with Daemon (turfd)\n"
                     "\n"
                     "The 'turf -H' command is C/S mode client interface, "
                     "executes command remotely\n"
                     "just like the same command in runc mode.\n"
                     "\n"
                     "COMMANDS supported in C/S mode\n"
                     "  create               Create a sandbox\n"
                     "  start                Start a sandbox\n"
                     "  stop                 Stop a sandbox\n"
                     "  delete               Delete a sandbox\n";

  int opt;
  while ((opt = getopt_long(argc, argv, so, lo, NULL)) > 0) {
    switch (opt) {
      case 'h':  // help
        puts(help);
        exit(0);
        break;

      case 'H':  // host
        cli->has.remote = 1;
        break;

      default:
        set_errno(EINVAL);
        return -1;
    };
  }

  if (optind == argc) {
    error("miss remote operation");
    set_errno(EINVAL);
    return -1;
  }

  int c = argc - optind;
  char* const* v = argv + optind;

  char* const cmd = v[0];
  dprint("cmd %s", cmd);

  // TBD: we should check all the parameters before remote call.
  if (strcmp(cmd, "create") == 0) {
    cli->cmd = TURF_CLI_CREATE;
    cli->argc = c;
    cli->argv = (char**)v;
  } else if (strcmp(cmd, "start") == 0) {
    cli->cmd = TURF_CLI_START;
    cli->argc = c;
    cli->argv = (char**)v;
  } else if (strcmp(cmd, "stop") == 0) {
    cli->cmd = TURF_CLI_STOP;
    cli->argc = c;
    cli->argv = (char**)v;
  } else if (strcmp(cmd, "delete") == 0) {
    cli->cmd = TURF_CLI_REMOVE;
    cli->argc = c;
    cli->argv = (char**)v;
  } else {
    set_errno(ENOTSUP);
    rc = -1;
  }

  return rc;
}

int cli_runc(tf_cli* cli, int argc, char* const argv[]) {
  char* const cmd = argv[1];

  // omit cmd begin of 'turf xxx'
  int c = argc - 1;
  char* const* v = argv + 1;

  int rc;
  if (strcmp(cmd, "info") == 0) {
    rc = cli_info(cli, c, v);
  } else if (strcmp(cmd, "spec") == 0) {
    rc = cli_spec(cli, c, v);
  } else if (strcmp(cmd, "create") == 0) {
    rc = cli_create(cli, c, v);
  } else if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0) {
    rc = cli_list(cli, c, v);
  } else if (strcmp(cmd, "ps") == 0) {
    rc = cli_ps(cli, c, v);
  } else if (strcmp(cmd, "start") == 0) {
    rc = cli_start(cli, c, v);
  } else if (strcmp(cmd, "stop") == 0) {
    rc = cli_stop(cli, c, v);
  } else if (strcmp(cmd, "state") == 0) {
    rc = cli_state(cli, c, v);
  } else if (strcmp(cmd, "delete") == 0) {
    rc = cli_delete(cli, c, v);
  } else if (strcmp(cmd, "init") == 0) {
    rc = cli_init(cli, c, v);
  } else if (strcmp(cmd, "run") == 0) {
    rc = cli_run(cli, c, v);
  } else if (strcmp(cmd, "runtime") == 0) {
    rc = cli_runtime(cli, c, v);
  } else {
    // unknown command
    printf("turf: '%s' is not a turf command.\n"
           "See 'turf --help'\n",
           cmd);
    exit(-1);
  }

  return rc;
}

// cli remote entry, called by daemon
int cli_parse_remote(tf_cli* cli, int argc, char* const argv[]) {
  int rc;

  if (argc < 2) {
    error("remote comamnd error.");
    exit(1);
  }

  optind = 0;
  char* const cmd = argv[1];

  // omit cmd begin of 'turf xxx'
  int c = argc - 1;
  char* const* v = argv + 1;

  if (strcmp(cmd, "create") == 0) {
    rc = cli_create(cli, c, v);
  } else if (strcmp(cmd, "start") == 0) {
    rc = cli_start(cli, c, v);
  } else if (strcmp(cmd, "stop") == 0) {
    rc = cli_stop(cli, c, v);
  } else if (strcmp(cmd, "delete") == 0) {
    rc = cli_delete(cli, c, v);
  } else {
    set_errno(ENOTSUP);
    error("unknown command %s", cmd);
    rc = -1;
  }

  // flag remote cli
  cli->has.remote = 1;
  return rc;
}

// cli main entry, called by main
int _API cli_parse(tf_cli* cli, int argc, char* const argv[]) {
  int rc;
  if (argc < 2) {
    cli_show_help();
    exit(1);
  }

  optind = 0;
  char* const cmd = argv[1];

  // C/S daemon cli
  if (strcmp(cmd, "-D") == 0) {
    rc = cli_daemon(cli, argc, argv);

    // show my version
  } else if ((strncmp(cmd, "--version", 9) == 0) ||
             (strncmp(cmd, "-v", 2) == 0)) {
    cli_show_version();
    exit(0);

    // C/S client cli
  } else if (strcmp(cmd, "-H") == 0) {  // parse rpc cmd
    rc = cli_remote(cli, argc, argv);

  } else if ((strncmp(cmd, "--help", 6) == 0) || (strncmp(cmd, "-h", 2) == 0)) {
    cli_show_help();
    exit(0);

    // parse runc cli
  } else {
    rc = cli_runc(cli, argc, argv);
  }

  return rc;
}

/**
 * @brief free all the memory during cli process.
 *
 * @param cli
 * @return int
 */
void _API cli_free(tf_cli* cli) {
  int i;

  if (!cli) {
    return;
  }

  if (cli->sandbox_name) {
    free(cli->sandbox_name);
    cli->sandbox_name = NULL;
  }

  if (cli->cwd) {
    free(cli->cwd);
    cli->cwd = NULL;
  }

  if (cli->bundle_path) {
    free(cli->bundle_path);
    cli->bundle_path = NULL;
  }

  if (cli->file_stdout) {
    free(cli->file_stdout);
    cli->file_stdout = NULL;
  }

  if (cli->file_stderr) {
    free(cli->file_stderr);
    cli->file_stderr = NULL;
  }

  if (cli->runtime_name) {
    free(cli->runtime_name);
    cli->runtime_name = NULL;
  }

  for (i = 0; i < cli->env_cnt; i++) {
    if (i >= TURF_CLI_MAX_ENV) {
      break;
    }
    if (cli->envs[i]) {
      // ssfree(cli->envs[i]);
      cli->envs[i] = NULL;
    }
  }

  if (cli->seed_sandbox_name) {
    free(cli->seed_sandbox_name);
    cli->seed_sandbox_name = NULL;
  }
}
