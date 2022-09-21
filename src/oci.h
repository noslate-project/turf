#ifndef _TURF_OCI_H_
#define _TURF_OCI_H_

#include "misc.h"
#include "stat.h"

#define TURF_OCI_VERSION "0.0.1-dev"

// represent a turf section in config.json
struct oci_spec_turf {
  struct {
    uint32_t os : 1;
    uint32_t runtime : 1;
    uint32_t code : 1;
    uint32_t binary : 1;
    uint32_t is_seed : 1;
  } has;

  char* os;       // node api version
  char* runtime;  // node api version
  char* code;     // path to code.zip
  char* binary;   // path to startup binary
  bool is_seed;   // warmfork cfg
};

// represent a process section in config.json
struct oci_spec_process {
  struct {
    uint32_t terminal : 1;
    uint32_t uid : 1;
    uint32_t gid : 1;
    uint32_t noNewPrivileges : 1;
  } has;

  int32_t terminal : 1;
  int32_t noNewPrivileges : 1;
  int uid;
  int gid;

  tf_arg* arg;
  tf_env* env;
};

struct oci_spec_root {
  struct {
    uint32_t path : 1;
    uint32_t readonly : 1;
  } has;

  char* path;
  int32_t readonly : 1;
};

struct oci_spec_linux_resources {
  struct {
    uint32_t mem_limit : 1;
    uint32_t cpu_shares : 1;
    uint32_t cpu_quota : 1;
    uint32_t cpu_period : 1;
  } has;

  int64_t mem_limit;
  int32_t cpu_shares;
  int32_t cpu_quota;
  int32_t cpu_period;
};

struct oci_spec_linux_seccomp {
  // TBD: support full seccomp spec.
  struct {
    uint32_t default_action : 1;
  } has;

  int default_action;  // SCMP_ACT_XXX
};

struct oci_spec_linux {
  struct {
    uint32_t resources : 1;
    uint32_t seccomp : 1;
  } has;
  struct oci_spec_linux_resources resources;
  struct oci_spec_linux_seccomp seccomp;
};

// config.json
struct oci_spec {
  struct {
    uint32_t process : 1;
    uint32_t root : 1;
    uint32_t linuxs : 1;
    uint32_t turf : 1;
  } has;

  struct oci_spec_process process;
  struct oci_spec_root root;
  struct oci_spec_linux linuxs;  // fix compile under gcc-5
  struct oci_spec_turf turf;
};

/*
{
    "ociVersion": "0.0.1",
    "id": "oci-container1",
    "status": "running",
    "pid": 4422,
    "bundle": "/containers/redis",
    "created": "2019-05-05T00:34:56.909051486Z",
    "stopped": "2019-05-05T00:35:00.909051486Z"
}
*/

// state.json
struct oci_state {
  char* id;
  uint32_t state;   // RLM_STATE_XXX
  uint32_t status;  // OOM, CPU_OVERLOAD, KILLED ...
  int pid;
  char* bundle;  // path to bundle dir
  struct timeval created;
  struct timeval stopped;

  struct {
    uint32_t exit_code : 1;
    uint32_t rusage : 1;
    uint32_t stat : 1;
  } has;

  int exit_code;
  struct rusage rusage;
  tf_stat stat;
};

// export api
// struct oci_spec _API *osi_spec_new(void);
// load spec from file
struct oci_spec _API* oci_spec_load(const char* path);
// load spec from string
struct oci_spec _API* oci_spec_loads(const char* json);
// save spec to string
int _API oci_spec_saves(struct oci_spec* cfg, char** json);
// save spec to file
int _API oci_spec_save(struct oci_spec* cfg, const char* path);
// free oci_spec
void _API oci_spec_free(struct oci_spec* cfg);

// create a state
struct oci_state _API* oci_state_create(const char* id, const char* bundle);
// load state from file
struct oci_state _API* oci_state_load(const char* path);
// save state to file
int _API oci_state_save(struct oci_state* state, const char* path);
// free state object
void _API oci_state_free(struct oci_state* state);

#endif  // _TURF_OCI_H_
