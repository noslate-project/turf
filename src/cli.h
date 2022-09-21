#ifndef _TURF_CLI_H_
#define _TURF_CLI_H_

#include "misc.h"

// all we supported cli intf
enum turf_cli_type_e {
  TURF_CLI_UNKOWN = 0,
  TURF_CLI_DAEMON,

  // start runc cli
  TURF_CLI_INIT = 10,
  TURF_CLI_SPEC,
  TURF_CLI_CREATE,
  TURF_CLI_REMOVE,
  TURF_CLI_LIST,
  TURF_CLI_STATE,
  TURF_CLI_EVENT,
  TURF_CLI_PS,
  TURF_CLI_START,
  TURF_CLI_STOP,
  TURF_CLI_RUN,
  TURF_CLI_INFO,
  TURF_CLI_RUNTIME,
  TURF_CLI_EVENTS,
};

// configure parsed by cli
struct tf_cli {
  int cmd;  // TURF_CLI_X
  struct {
    int remote : 1;      // C/S flag
    int foreground : 1;  // --foreground flag, C/S deamon for debug purpose
    int cpulimit : 1;    // --cpu flag
    int memlimit : 1;    // --mem flag
    int kill_sig : 1;    // --kill flag
    int force : 1;       // --force flag
    int all : 1;         // --all flag
    int install : 1;     // --install flag
    int time_wait : 1;   // --time int
    int seed : 1;        // --seed flag
  } has;

  // char *workdir;          // turf workdir, for multiple instance
  uint32_t memlimit;  // limit in megabyte, 10 = 10MB
  uint32_t cpulimit;  // limit in cpu_time(ms), 1000ms means 100% of one core.
  uint32_t kill_sig;  // kill with the signal
#define TURF_CLI_DEFAULT_TIME_WAIT 3
  uint32_t time_wait;  // time to wait, in stop/kill

  char* sandbox_name;  // sandbox name
  char* cwd;           // hold a path to current workdir.
  char* bundle_path;   // hold a path to bundle dir path.
  char* spec_path;     // hold a path to specification file path.
  char* file_stdout;   // write stdout to file.
  char* file_stderr;   // write stderr to file.
  char* runtime_name;  // runtime name
#define TURF_CLI_MAX_ENV 10
  char* envs[TURF_CLI_MAX_ENV];  // user specified environ values
  uint32_t env_cnt;              // envs counter
  int argc;                      // user arguments
  char** argv;
  char* seed_sandbox_name;  // the seed sandbox name, warm-fork
};

typedef struct tf_cli tf_cli;

// global cli parse
int _API cli_parse(tf_cli* cli, int argc, char* const argv[]);

// remote call cli parse
int _API cli_parse_remote(tf_cli* cli, int argc, char* const argv[]);

// show help (no exit)
void _API cli_show_help(void);

// show version (no exit)
void _API cli_show_version(void);

// free cli resources
void _API cli_free(tf_cli* cli);

// inner
int cli_get_uint32(uint32_t* v, const char* str, uint32_t def);
int cli_get_uint64(uint64_t* v, const char* str, uint64_t def);

#endif  // _TURF_CLI_H_
