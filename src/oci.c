
/* support oci intrf
 */

#include "oci.h"
#include "cjson/cJSON.h"
#include "realm.h"  // rlm_state_str()

/*
 * https://github.com/opencontainers/runtime-spec/blob/master/config.md
 */

static int oci_parse_args(tf_arg** parg, const cJSON* array) {
  int rc = 0;
  int size = cJSON_GetArraySize(array);
  tf_arg* arg = arg_new(0);
  if (!arg) {
    return -1;
  }

  int i;
  for (i = 0; i < size; i++) {
    cJSON* k = cJSON_GetArrayItem(array, i);
    if (k) {
      rc = arg_add(arg, k->valuestring);
      if (rc < 0) {
        arg_free(arg);
        return -1;
      }
    }
  }

  *parg = arg;
  return 0;
}

static int oci_parse_env(tf_env** penv, const cJSON* array) {
  int rc = 0;
  int size = cJSON_GetArraySize(array);
  tf_env* env = env_new(0);
  if (!env) {
    return -1;
  }

  int i;
  for (i = 0; i < size; i++) {
    cJSON* k = cJSON_GetArrayItem(array, i);
    if (k) {
      rc = env_add(env, k->valuestring);
      if (rc < 0) {
        env_free(env);
        return -1;
      }
    }
  }

  *penv = env;
  return 0;
}

struct oci_spec _API* oci_spec_loads(const char* json) {
  cJSON* j = NULL;  // hold the json
  cJSON* k = NULL;  // tmp for leaf/key
  bool success = 0;
  struct oci_spec* cfg = NULL;

  j = cJSON_Parse(json);
  if (!j) {
    goto exit;
  }

  cfg = (struct oci_spec*)calloc(1, sizeof(struct oci_spec));
  if (!cfg) {
    goto exit;
  }

  // parse process
  {
    cJSON* process = cJSON_GetObjectItem(j, "process");
    k = cJSON_GetObjectItem(process, "terminal");
    if (k) {
      cfg->process.terminal = k->valueint;
      cfg->process.has.terminal = 1;
    }

    cJSON* user = cJSON_GetObjectItem(process, "user");
    if (user) {
      k = cJSON_GetObjectItem(user, "uid");
      if (k) {
        cfg->process.uid = k->valueint;
        cfg->process.has.uid = 1;
      }
      k = cJSON_GetObjectItem(user, "gid");
      if (k) {
        cfg->process.gid = k->valueint;
        cfg->process.has.gid = 1;
      }
    }

    cJSON* args = cJSON_GetObjectItem(process, "args");
    if (args) {
      oci_parse_args(&cfg->process.arg, args);
    }

    cJSON* env = cJSON_GetObjectItem(process, "env");
    if (env) {
      oci_parse_env(&cfg->process.env, env);
    }

    k = cJSON_GetObjectItem(process, "noNewPrivileges");
    if (k) {
      cfg->process.noNewPrivileges = k->valueint;
      cfg->process.has.noNewPrivileges = 1;
    }

    cfg->has.process = 1;
  }

  // parse root
  {
    cJSON* root = cJSON_GetObjectItem(j, "root");
    if (root) {
      k = cJSON_GetObjectItem(root, "path");
      if (k) {
        cfg->root.path = strdup(k->valuestring);
        cfg->root.has.path = 1;
      }
      k = cJSON_GetObjectItem(root, "readonly");
      if (k) {
        cfg->root.readonly = k->valueint;
        cfg->root.has.readonly = 1;
      }

      cfg->has.root = 1;
    }
  }

  // parse linux
  {
    cJSON* linuxs = cJSON_GetObjectItem(j, "linux");
    if (linuxs) {
      cJSON* resources = cJSON_GetObjectItem(linuxs, "resources");
      if (resources) {
        cJSON* memory = cJSON_GetObjectItem(resources, "memory");
        if (memory) {
          k = cJSON_GetObjectItem(memory, "limit");
          if (k) {
            cfg->linuxs.resources.mem_limit = k->valueint;
            cfg->linuxs.resources.has.mem_limit = 1;
          }
        }

        cJSON* cpu = cJSON_GetObjectItem(resources, "cpu");
        if (cpu) {
          k = cJSON_GetObjectItem(cpu, "shares");
          if (k) {
            cfg->linuxs.resources.cpu_shares = k->valueint;
            cfg->linuxs.resources.has.cpu_shares = 1;
          }

          k = cJSON_GetObjectItem(cpu, "quota");
          if (k) {
            cfg->linuxs.resources.cpu_quota = k->valueint;
            cfg->linuxs.resources.has.cpu_quota = 1;
          }

          k = cJSON_GetObjectItem(cpu, "period");
          if (k) {
            cfg->linuxs.resources.cpu_period = k->valueint;
            cfg->linuxs.resources.has.cpu_period = 1;
          }
        }

        cfg->linuxs.has.resources = 1;
      }

      cfg->has.linuxs = 1;
    }
  }

  // parse turf
  {
    cJSON* turf = cJSON_GetObjectItem(j, "turf");
    if (turf) {
      k = cJSON_GetObjectItem(turf, "os");
      if (k) {
        cfg->turf.os = strdup(k->valuestring);
        cfg->turf.has.os = 1;
      }

      k = cJSON_GetObjectItem(turf, "runtime");
      if (k) {
        cfg->turf.runtime = strdup(k->valuestring);
        cfg->turf.has.runtime = 1;
      }

      k = cJSON_GetObjectItem(turf, "code");
      if (k) {
        cfg->turf.code = strdup(k->valuestring);
        cfg->turf.has.code = 1;
      }

      k = cJSON_GetObjectItem(turf, "seed");
      if (k) {
        cfg->turf.is_seed = cJSON_IsTrue(k);
        cfg->turf.has.is_seed = 1;
      }

      cfg->has.turf = 1;
    }
  }
  success = 1;

exit:
  // release cJSON resources
  if (j) {
    cJSON_Delete(j);
  }

  // good return
  if (success) {
    return cfg;
  }

  // error return
  if (cfg) {
    oci_spec_free(cfg);
  }
  return NULL;
}

struct oci_spec _API* oci_spec_load(const char* path) {
  int rc;
  char* json = NULL;
  size_t json_size = 0;
  struct oci_spec* cfg = NULL;
  bool success = 0;

  rc = read_file(path, &json, &json_size);
  if (rc < 0) {
    return NULL;
  }

  cfg = oci_spec_loads(json);
  if (!cfg) {
    goto exit;
  }
  success = 1;

exit:
  // release file buffer
  if (json) {
    free(json);
  }

  // good return
  if (success) {
    return cfg;
  }

  // error return
  return NULL;
}

// save oci_spec to json string
int _API oci_spec_saves(struct oci_spec* cfg, char** json) {
  int i;
  bool success = 0;

  if (!cfg || !json) {
    set_errno(EINVAL);
    return -1;
  }

  cJSON* j = cJSON_CreateObject();
  if (!j) {
    goto exit;
  }

  // put ociVersion
  if (cJSON_AddStringToObject(j, "ociVersion", TURF_OCI_VERSION) == NULL) {
    goto exit;
  }

  // put process
  if (cfg->has.process) {
    cJSON* process = cJSON_CreateObject();
    if (process == NULL) {
      goto exit;
    }

    // terminal
    if (cfg->process.has.terminal) {
      if (cJSON_AddBoolToObject(process, "terminal", cfg->process.terminal) ==
          NULL) {
        goto exit;
      }
    }

    // user
    if (cfg->process.has.uid || cfg->process.has.gid) {
      cJSON* user = cJSON_CreateObject();
      if (!user) {
        goto exit;
      }

      // uid
      if (cfg->process.has.uid) {
        if (cJSON_AddNumberToObject(user, "uid", (int)cfg->process.uid) ==
            NULL) {
          goto exit;
        }
      }

      // gid
      if (cfg->process.has.gid) {
        if (cJSON_AddNumberToObject(user, "gid", (int)cfg->process.gid) ==
            NULL) {
          goto exit;
        }
      }
      cJSON_AddItemToObject(process, "user", user);
    }

    // args
    {
      cJSON* args = cJSON_CreateArray();
      if (args == NULL) {
        goto exit;
      }

      if (cfg->process.arg) {
        for (i = 0; i < cfg->process.arg->argc; ++i) {
          cJSON* s = cJSON_CreateString(cfg->process.arg->argv[i]);
          if (s == NULL) {
            goto exit;
          }
          if (cJSON_AddItemToArray(args, s) == 0) {
            goto exit;
          }
        }
      }
      cJSON_AddItemToObject(process, "args", args);
    }

    // env
    {
      cJSON* env = cJSON_CreateArray();
      if (env == NULL) {
        goto exit;
      }

      if (cfg->process.env) {
        for (i = 0; i < cfg->process.env->size; ++i) {
          cJSON* s = cJSON_CreateString(cfg->process.env->environ[i]);
          if (s == NULL) {
            goto exit;
          }
          if (cJSON_AddItemToArray(env, s) == 0) {
            goto exit;
          }
        }
      }

      cJSON_AddItemToObject(process, "env", env);
    }

    // noNewPrivileges
    if (cfg->process.has.noNewPrivileges) {
      if (cJSON_AddBoolToObject(process,
                                "noNewPrivileges",
                                cfg->process.noNewPrivileges) == NULL) {
        goto exit;
      }
    }

    cJSON_AddItemToObject(j, "process", process);
  }

  // put root
  if (cfg->has.root) {
    cJSON* root = cJSON_CreateObject();
    if (root == NULL) {
      goto exit;
    }

    // root.path str
    if (cfg->root.has.path) {
      if (cJSON_AddStringToObject(root, "path", cfg->root.path) == NULL) {
        goto exit;
      }
    }

    // root.readonly bool
    if (cfg->root.has.readonly) {
      if (cJSON_AddBoolToObject(root, "readonly", cfg->root.readonly) == NULL) {
        goto exit;
      }
    }

    cJSON_AddItemToObject(j, "root", root);
  }

  // put linux
  if (cfg->has.linuxs) {
    cJSON* lnx = cJSON_CreateObject();
    if (lnx == NULL) {
      goto exit;
    }

    // linux.resources
    if (cfg->linuxs.has.resources) {
      cJSON* resources = cJSON_CreateObject();
      if (resources == NULL) {
        goto exit;
      }

      // resources.memory.limit
      if (cfg->linuxs.resources.has.mem_limit) {
        cJSON* memory = cJSON_CreateObject();
        if (memory == NULL) {
          goto exit;
        }

        if (cJSON_AddNumberToObject(
                memory, "limit", cfg->linuxs.resources.mem_limit) == NULL) {
          goto exit;
        }

        cJSON_AddItemToObject(resources, "memory", memory);
      }

      // resources.cpu
      if (cfg->linuxs.resources.has.cpu_shares ||
          cfg->linuxs.resources.has.cpu_quota ||
          cfg->linuxs.resources.has.cpu_period) {
        cJSON* cpu = cJSON_CreateObject();
        if (cpu == NULL) {
          goto exit;
        }

        // cpu.cpu_shares
        if (cfg->linuxs.resources.cpu_shares) {
          if (cJSON_AddNumberToObject(
                  cpu, "shares", cfg->linuxs.resources.cpu_shares) == NULL) {
            goto exit;
          }
        }

        // cpu.cpu_quota
        if (cfg->linuxs.resources.cpu_quota) {
          if (cJSON_AddNumberToObject(
                  cpu, "quota", cfg->linuxs.resources.cpu_quota) == NULL) {
            goto exit;
          }
        }

        // cpu.cpu_period
        if (cfg->linuxs.resources.cpu_period) {
          if (cJSON_AddNumberToObject(
                  cpu, "period", cfg->linuxs.resources.cpu_period) == NULL) {
            goto exit;
          }
        }

        cJSON_AddItemToObject(resources, "cpu", cpu);
      }

      cJSON_AddItemToObject(lnx, "resources", resources);
    }

    if (cfg->linuxs.has.seccomp) {
      // TBD: support full seccomp sepc.
    }

    cJSON_AddItemToObject(j, "linux", lnx);
  }

  // put turf
  if (cfg->has.turf) {
    cJSON* turf = cJSON_CreateObject();
    if (turf == NULL) {
      goto exit;
    }

    // turf.os str
    if (cfg->turf.has.os) {
      if (cJSON_AddStringToObject(turf, "os", cfg->turf.os) == NULL) {
        goto exit;
      }
    }

    // turf.runtime str
    if (cfg->turf.has.runtime) {
      if (cJSON_AddStringToObject(turf, "runtime", cfg->turf.runtime) == NULL) {
        goto exit;
      }
    }

    // turf.code str
    if (cfg->turf.has.code) {
      if (cJSON_AddStringToObject(turf, "code", cfg->turf.code) == NULL) {
        goto exit;
      }
    }

    // turf.binary str
    if (cfg->turf.has.binary) {
      if (cJSON_AddStringToObject(turf, "binary", cfg->turf.binary) == NULL) {
        goto exit;
      }
    }

    // turf.is_seed bool
    if (cfg->turf.has.is_seed) {
      if (cJSON_AddBoolToObject(turf, "seed", cfg->turf.is_seed) == NULL) {
        goto exit;
      }
    }

    cJSON_AddItemToObject(j, "turf", turf);
  }

  // printfy
  *json = cJSON_PrintUnformatted(j);
  if (!*json) {
    goto exit;
  }

  success = 1;

exit:
  if (j) {
    cJSON_Delete(j);
  }

  if (success) {
    return 0;
  }

  return -1;
}

// save oci_spec to file
int _API oci_spec_save(struct oci_spec* cfg, const char* path) {
  int rc;
  char* json = NULL;
  size_t json_size = 0;
  bool success = 0;

  rc = oci_spec_saves(cfg, &json);
  if (rc < 0) {
    goto exit;
  }

  if (!json) {
    goto exit;
  }

  json_size = strlen(json);

  rc = write_file(path, json, json_size);
  if (rc < 0) {
    goto exit;
  }

  success = 1;

exit:
  // release file buffer
  if (json) {
    free(json);
  }

  // good return
  if (success) {
    return 0;
  }

  // error return
  return -1;
}

// free the spec
void oci_spec_free(struct oci_spec* cfg) {
  if (!cfg) {
    return;
  }

  if (cfg->has.process) {
    arg_free(cfg->process.arg);
    env_free(cfg->process.env);
  }

  if (cfg->has.linuxs) {
    // no string
  }

  if (cfg->has.root) {
    if (cfg->root.has.path && cfg->root.path) {
      free(cfg->root.path);
      cfg->root.path = NULL;
    }
  }

  if (cfg->has.turf) {
    if (cfg->turf.has.os && cfg->turf.os) {
      free(cfg->turf.os);
      cfg->turf.os = NULL;
    }
    if (cfg->turf.has.runtime && cfg->turf.runtime) {
      free(cfg->turf.runtime);
      cfg->turf.runtime = NULL;
    }
    if (cfg->turf.has.code && cfg->turf.code) {
      free(cfg->turf.code);
      cfg->turf.code = NULL;
    }
  }

  free(cfg);
}

struct oci_state _API* oci_state_create(const char* name, const char* bundle) {
  struct oci_state* state =
      (struct oci_state*)calloc(1, sizeof(struct oci_state));
  if (!state) {
    return NULL;
  }

  state->id = strdup(name);
  state->bundle = strdup(bundle);

  return state;
}

struct oci_state _API* oci_state_load(const char* path) {
  int rc = -1;
  char* json = NULL;
  size_t json_size = 0;
  cJSON* j = NULL;  // hold the json
  cJSON* k = NULL;  // tmp for leaf/key
  bool success = 0;
  struct oci_state* state = NULL;

  rc = read_file(path, &json, &json_size);
  if (rc < 0) {
    goto exit;
  }

  j = cJSON_Parse(json);
  if (!j) {
    goto exit;
  }

  state = (struct oci_state*)calloc(1, sizeof(struct oci_state));
  if (!state) {
    goto exit;
  }

  k = cJSON_GetObjectItem(j, "id");
  if (k) {
    state->id = strdup(k->valuestring);
  }

  k = cJSON_GetObjectItem(j, "state");
  if (k) {
    state->state = rlm_state(k->valuestring);
  }

  k = cJSON_GetObjectItem(j, "status");
  if (k) {
    state->status = k->valueint;
  }

  k = cJSON_GetObjectItem(j, "pid");
  if (k) {
    state->pid = k->valueint;
  }

  k = cJSON_GetObjectItem(j, "bundle");
  if (k) {
    state->bundle = strdup(k->valuestring);
  }

  k = cJSON_GetObjectItem(j, "created");
  if (k) {
    if (str2timeval(k->valuestring, &state->created) < 0) {
      goto exit;
    }
  }

  k = cJSON_GetObjectItem(j, "stopped");
  if (k) {
    if (str2timeval(k->valuestring, &state->stopped) < 0) {
      goto exit;
    }
  }

  // exit_code
  k = cJSON_GetObjectItem(j, "exit_code");
  if (k) {
    state->exit_code = k->valueint;
    state->has.exit_code = 1;
  }

  // rusage
  k = cJSON_GetObjectItem(j, "rusage");
  if (k) {
    cJSON* v;
    v = cJSON_GetObjectItem(k, "utime");
    if (v) {
      ms2timeval(v->valueint, &state->rusage.ru_utime);
    }
    v = cJSON_GetObjectItem(k, "stime");
    if (v) {
      ms2timeval(v->valueint, &state->rusage.ru_stime);
    }
    v = cJSON_GetObjectItem(k, "maxrss");
    if (v) {
      state->rusage.ru_maxrss = v->valueint;
    }
    state->has.rusage = 1;
  }

  success = 1;

exit:
  if (j) {
    cJSON_Delete(j);
  }
  if (json) {
    free(json);
  }

  // good return
  if (success) {
    return state;
  }

  // error return
  if (state) {
    free(state);
  }
  return NULL;
}

int _API oci_state_save(struct oci_state* state, const char* path) {
  int rc = -1;
  cJSON* j = NULL;
  char* json = NULL;
  bool success = 0;
  char* time_created = NULL;
  char* time_stopped = NULL;

  j = cJSON_CreateObject();
  if (!j) {
    goto exit;
  }

  if (cJSON_AddStringToObject(j, "ociVersion", TURF_OCI_VERSION) == NULL) {
    goto exit;
  }

  if (cJSON_AddStringToObject(j, "id", state->id) == NULL) {
    goto exit;
  }

  if (cJSON_AddNumberToObject(j, "pid", state->pid) == NULL) {
    goto exit;
  }

  if (cJSON_AddStringToObject(j, "bundle", state->bundle) == NULL) {
    goto exit;
  }

  if (cJSON_AddStringToObject(j, "state", rlm_state_str(state->state)) ==
      NULL) {
    goto exit;
  }

  if (cJSON_AddNumberToObject(j, "status", state->status) == NULL) {
    goto exit;
  }

  time_created = timeval2str(&state->created);
  if (time_created) {
    if (cJSON_AddStringToObject(j, "created", time_created) == NULL) {
      goto exit;
    }
  }

  time_stopped = timeval2str(&state->stopped);
  if (time_stopped) {
    if (cJSON_AddStringToObject(j, "stopped", time_stopped) == NULL) {
      goto exit;
    }
  }

  // exit_code
  if (state->has.exit_code) {
    dprint("exit_code");
    if (cJSON_AddNumberToObject(j, "exit_code", state->exit_code) == NULL) {
      goto exit;
    }
  }

  // rusage
  if (state->has.rusage) {
    cJSON* r = cJSON_CreateObject();
    if (!r) {
      goto exit;
    }

    if (cJSON_AddNumberToObject(
            r, "utime", timeval2ms(&state->rusage.ru_utime)) == NULL) {
      goto exit;
    }

    if (cJSON_AddNumberToObject(
            r, "stime", timeval2ms(&state->rusage.ru_stime)) == NULL) {
      goto exit;
    }

    if (cJSON_AddNumberToObject(r, "maxrss", state->rusage.ru_maxrss) == NULL) {
      goto exit;
    }

    cJSON_AddItemToObject(j, "rusage", r);
  }

  json = cJSON_PrintUnformatted(j);
  if (!json) {
    goto exit;
  }

  rc = write_file(path, json, strlen(json));
  if (rc < 0) {
    goto exit;
  }

  success = 1;

exit:
  if (j) {
    cJSON_Delete(j);
  }
  if (json) {
    free(json);
  }
  if (time_created) {
    free(time_created);
  }
  if (time_stopped) {
    free(time_stopped);
  }
  return success ? 0 : -1;
}

void _API oci_state_free(struct oci_state* state) {
  if (!state) {
    return;
  }

  if (state->id) {
    free(state->id);
    state->id = NULL;
  }

  if (state->bundle) {
    free(state->bundle);
    state->bundle = NULL;
  }

  free(state);
}
