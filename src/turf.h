#ifndef _TURF_TURF_H_
#define _TURF_TURF_H_

#include "cli.h"    // struct tf_cli
#include "realm.h"  // struct rlm_t
#include "stat.h"
#include "warmfork.h"  // struct tf_seed

// represent a running turf sandbox
struct turf_t {
  struct rlm_t* realm;            // the realm core
#define pid_ realm->st.child_pid  // alias of tf.pid
#define name_ realm->cfg.name     // alias of tf.name

  uint32_t state;   // TF_STATE_XXX
  uint32_t status;  // OOM, COL, KLL ...

  // oci intf
  struct oci_spec* i_spec;    // holds oci_spec
  struct oci_state* o_state;  // holds oci_state

  // global list
  LIST_ENTRY(turf_t) in_list;  // in global sandbox list

  // stat
  struct tf_stat stat;           // stat: io, cpu, mem ...
  int cont_mem_ovl;              // continue memory overload
  int cont_cpu_ovl;              // continue cpu overload
  long last_cpu_used;            // for cpu time calc
  struct timeval last_cpu_time;  // for cpu time calc

  // warmfork's seed
  struct tf_seed seed;  // holds the seed process
};

// turf entry function
int _API tf_action(struct tf_cli* cfg);

int _API tf_health_check(void);
int _API tf_child_exit(pid_t child, struct rusage* ru, int exit_code);
int _API tf_exit_all(void);

#endif  // _TURF_TURF_H_
