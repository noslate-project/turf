/* support shell function,
 *   environ, file access ... etc
 */
#include "shell.h"

// static const char *m_workdir = NULL;

// getenv('HOME')
const char* env_home() {
  static char* home = NULL;
  if (!home) {
    home = getenv("HOME");
  }
  return home;
}

// getenv('TURF_WORKDIR')
const char* env_workdir() {
  static char* p = NULL;
  if (p) {
    return p;
  }
  char* dir = getenv("TURF_WORKDIR");
  if (!dir || dir[0] != '/' || strlen(dir) < 2) {
    return NULL;
  }
  p = dir;
  return p;
}

#if 0
int _API tfd_set_workdir(const char *workdir)
{
    m_workdir = strdup(workdir);
    return 0;
}
#endif

const char _API* tfd_path() {
  static char* p = NULL;
  if (p) {
    return p;
  }
  const char* dir = env_workdir();
  if (dir) {  // using getenv("TURF_WORKDIR")
    p = strdup(dir);
  } else {  // using ~/.turf
    xasprintf(&p, "%s/.turf", env_home());
  }
  return p;
}

const char _API* tfd_path_sandbox() {
  static char* p = NULL;
  if (!p) {
    xasprintf(&p, "%s/sandbox", tfd_path());
  }
  return p;
}

const char _API* tfd_path_runtime() {
  static char* p = NULL;
  if (!p) {
    xasprintf(&p, "%s/runtime", tfd_path());
  }
  return p;
}

const char _API* tfd_path_overlay() {
  static char* p = NULL;
  if (!p) {
    xasprintf(&p, "%s/overlay", tfd_path());
  }
  return p;
}

const char _API* tfd_path_sock() {
  static char* p = NULL;
  if (!p) {
    xasprintf(&p, "%s/%s", tfd_path(), "turf.sock");
  }
  return p;
}

const char _API* tfd_path_libturf() {
  static char* p = NULL;

  if (p) {
    return p;
  }

  p = getenv("LIBTURF_PATH");
  if (p && (strlen(p) > 3)) {
    int rc = access(p, X_OK);
    if (rc == 0) {
      return p;
    } else {
      error("invalid LIBTURF_PATH.");
      // the path cannot be used as libturf_path.
    }
  }

  xasprintf(&p, "%s/libturf.so", tfd_path());
  return p;
}

// shell utilities
int _API shl_access2(const char* parent, const char* dir) {
  char path[TURF_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", parent, dir);
  return access(path, F_OK);
}

int _API shl_access3(const char* parent, const char* sbx, const char* dir) {
  char path[TURF_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s/%s", parent, sbx, dir);
  return access(path, F_OK);
}

int _API shl_access4(const char* parent,
                     const char* sbx,
                     const char* dir,
                     const char* file) {
  char path[TURF_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s/%s/%s", parent, sbx, dir, file);
  return access(path, F_OK);
}

bool _API shl_exist2(const char* parent, const char* dir) {
  int rc = shl_access2(parent, dir);
  if (rc == 0) {
    return 1;
  }
  return 0;
}

bool _API shl_notexist2(const char* parent, const char* dir) {
  int rc = shl_access2(parent, dir);
  if (rc < 0 && errno == ENOENT) {
    return 1;
  }
  return 0;
}

bool _API shl_exist3(const char* parent, const char* sbx, const char* dir) {
  int rc = shl_access3(parent, sbx, dir);
  if (rc == 0) {
    return 1;
  }
  return 0;
}

bool _API shl_notexitst3(const char* parent, const char* sbx, const char* dir) {
  int rc = shl_access3(parent, sbx, dir);
  if (rc < 0 && errno == ENOENT) {
    return 1;
  }
  return 0;
}

bool _API shl_exist4(const char* parent,
                     const char* sbx,
                     const char* dir,
                     const char* file) {
  int rc = shl_access4(parent, sbx, dir, file);
  if (rc == 0) {
    return 1;
  }
  return 0;
}

int _API shl_mkdir(const char* dir) {
  return mkdir(dir, 0775);
}

int _API shl_mkdir2(const char* parent, const char* dir) {
  char path[TURF_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s", parent, dir);
  return mkdir(path, 0775);
}

int _API shl_mkdir3(const char* parent, const char* sbx, const char* dir) {
  char path[TURF_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s/%s/%s", parent, sbx, dir);
  return mkdir(path, 0775);
}

int _API shl_path2(char* buf,
                   size_t size,
                   const char* parent,
                   const char* dir) {
  snprintf(buf, size, "%s/%s", parent, dir);
  return 0;
}

int _API shl_path3(char* buf,
                   size_t size,
                   const char* parent,
                   const char* sbx,
                   const char* dir) {
  snprintf(buf, size, "%s/%s/%s", parent, sbx, dir);
  return 0;
}

int _API shl_path4(char* buf,
                   size_t size,
                   const char* parent,
                   const char* sbx,
                   const char* dir,
                   const char* file) {
  snprintf(buf, size, "%s/%s/%s/%s", parent, sbx, dir, file);
  return 0;
}

int _API shl_cp(const char* src, const char* dest, const char* name) {
  char cmd[TURF_MAX_CMD_LEN];
  if (name) {  // with filename
    snprintf(cmd, sizeof(cmd), "cp -Rf '%s' '%s/%s'", src, dest, name);
  } else {
    snprintf(cmd, sizeof(cmd), "cp -Rf '%s' '%s'", src, dest);
  }
  return system(cmd);
}

int _API shl_rmdir2(const char* parent, const char* dir) {
  char cmd[TURF_MAX_CMD_LEN];

  // TBD: very dangerous op.
  if ((strlen(parent) + strlen(dir)) < 10) {
    return -1;
  }

  snprintf(cmd, sizeof(cmd), "rm -r '%s/%s'", parent, dir);
  return system(cmd);
}
