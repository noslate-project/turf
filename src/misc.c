#include "misc.h"
#include <sys/time.h>  // gettimeofday()

// return a  formated string, should use free() to release.
int _API xasprintf(char** str, const char* fmt, ...) {
  va_list args_list;

  va_start(args_list, fmt);

  int rc = vasprintf(str, fmt, args_list);
  if (UNLIKELY(rc < 0)) {
    die_oom();
  }

  va_end(args_list);

  return rc;
}

int _API get_current_time(struct timeval* tv) {
  return gettimeofday(tv, NULL);
}

// formated timeval string, need to free() after use.
char _API* timeval2str(struct timeval* tv) {
  char* out = NULL;
  struct tm val = {0};
  char buf[64];

  // utc
  gmtime_r(&tv->tv_sec, &val);

  strftime(buf, 64, "%Y-%m-%dT%H:%M:%S", &val);
  xasprintf(&out, "%s.%09ldZ", buf, tv->tv_usec);

  return out;
}

// covert timeval to microseconds, loss accuracy
int64_t _API timeval2ms(const struct timeval* tv) {
  int64_t ms = (tv->tv_sec * 1000) + (tv->tv_usec / 1000);
  return ms;
}

// convert string to timeval
int _API str2timeval(const char* str, struct timeval* tv) {
  if (!str || !tv) {
    set_errno(EINVAL);
    return -1;
  }
  char buf[32];
  struct tm val = {0};
  long usec = 0;

  sscanf(str, "%19s.%9ldZ", buf, &usec);
  strptime(buf, "%Y-%m-%dT%H:%M:%S", &val);

  // utc
  time_t sec = timegm(&val);

  tv->tv_sec = sec;
  tv->tv_usec = usec;
  return 0;
}

// convert ms to timeval
int _API ms2timeval(int64_t ms, struct timeval* tv) {
  tv->tv_sec = ms / 1000;
  tv->tv_usec = (ms % 1000) * 1000;
  return 0;
}

// return buffur pinter to file context, should use free() to release.
int _API read_file(const char* path, char** pbuf, size_t* psize) {
  int rc = -1;
  char* buf = NULL;
  int fd = -1;
  size_t size = 0;
  struct stat st = {0};

  if (!path || !pbuf || !psize) {
    set_errno(EINVAL);
    return -1;
  }

  rc = stat(path, &st);
  if (rc < 0) {
    return rc;
  }

  size = st.st_size;
  *psize = size;

  if (size > TURF_MAX_FILE_SIZE) {
    set_errno(E2BIG);
    return -1;
  }

  /* some of functions uses the function to read text from file,
   * and it will attend to read size beyond buffer size,
   * so it good to have '\0' char to terminal the string.
   */
  buf = (char*)malloc(size + 1);
  if (!buf) {
    set_errno(ENOMEM);
    return -1;
  }
  buf[size] = 0;

  rc = open(path, O_RDONLY | O_CLOEXEC);
  if (rc < 0) {
    goto error;
  }
  fd = rc;

  rc = read(fd, buf, size);
  if (rc < 0) {
    goto error;
  }

  *pbuf = buf;
  close(fd);
  return 0;

error:
  if (buf) {
    free(buf);
  }
  if (fd > 0) {
    close(fd);
  }
  return rc;
}

int _API write_file(const char* path, const char* buf, size_t size) {
  int rc = -1;
  int fd = -1;

  if (!path || !buf) {
    set_errno(EINVAL);
    return -1;
  }

  rc = open(path, O_CREAT | O_TRUNC | O_WRONLY | O_CLOEXEC, 0644);
  if (rc < 0) {
    return rc;
  }

  fd = rc;
  rc = write(fd, buf, size);
  if (rc != (int)size) {
    goto exit;
  }

  rc = 0;
exit:
  if (fd > 0) {
    close(fd);
  }
  return rc;
}

int _API proc_write(const char* path, const char* str, size_t len) {
  int f = open(path, O_WRONLY | O_CLOEXEC);

  if (f < 0) {
    die("open_proc");
  }

  size_t rc = write(f, str, len);
  if (rc != len) {
    die("write_proc");
  }

  if (f) {
    close(f);
  }

  return rc;
}

int _API proc_pid_max(int* pid_max) {
  char* buf = NULL;
  size_t size = 0;

  if (!pid_max) {
    set_errno(EINVAL);
    return -1;
  }

  int rc = read_file("/proc/sys/kernel/pid_max", &buf, &size);
  if (rc) {
    return rc;
  }

  *pid_max = atoi(buf);

  if (buf) {
    free(buf);
  }
  return 0;
}

#if DEBUG
void _hexbuf(const char* buf, int size) {
  int i;
  for (i = 0; i < size; i++) {
    fprintf(stderr, "%02x ", (uint8_t)buf[i]);
    if (i && i % 8 == 0) {
      fprintf(stderr, " ");
    } else if (i && i % 16 == 0) {
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "\n");
}
#endif

void _API rusage_dump(const struct rusage* ru) {
  info("rusage info:\n"
       "utime: %ld\n"
       "stime: %ld\n"
       "maxrss: %ld\n"
       "ixrss: %ld\n"
       "idrss: %ld\n"
       "isrss: %ld\n"
       "minflt: %ld\n"
       "majflt: %ld\n"
       "nswap: %ld\n"
       "inblock: %ld\n"
       "oublock: %ld\n"
       "msgsnd: %ld\n"
       "msgrcv: %ld\n"
       "nsignals: %ld\n"
       "nvcsw: %ld\n"
       "nivcsw: %ld\n",
       (long)timeval2ms(&ru->ru_utime),
       (long)timeval2ms(&ru->ru_stime),
       ru->ru_maxrss,
       ru->ru_ixrss,
       ru->ru_idrss,
       ru->ru_isrss,
       ru->ru_minflt,
       ru->ru_majflt,
       ru->ru_nswap,
       ru->ru_inblock,
       ru->ru_oublock,
       ru->ru_msgsnd,
       ru->ru_msgrcv,
       ru->ru_nsignals,
       ru->ru_nvcsw,
       ru->ru_nivcsw);
}

str_t _API* str_new(const char* s) {
  if (!s) {
    set_errno(EINVAL);
    return NULL;
  }
  str_t* str = calloc(1, sizeof(str_t));
  if (!str) {
    set_errno(ENOMEM);
    return NULL;
  }
  str->_magic = TF_STR_MAGIC;
  str->str = strndup(s, TURF_MAX_STR_LEN);
  str->_ref = 1;  // initialized reference count

  return str;
}

str_t _API* str_copy(str_t* from) {
  if (!from || BAD_MAGIC(from->_magic, TF_STR_MAGIC)) {
    set_errno(EINVAL);
    return NULL;
  }

  if (from->_ref <= 0) {
    set_errno(EOVERFLOW);
    return NULL;
  }

  from->_ref++;
  return from;
}

void _API str_free(str_t* s) {
  if (!s || BAD_MAGIC(s->_magic, TF_STR_MAGIC)) {
    set_errno(EINVAL);
    return;
  }

  assert(s->str);
  assert(s->_ref > 0);

  if (s->_ref == 1) {
    free(s->str);
    s->str = NULL;
    s->_ref = 0;
    free(s);
    return;
  }

  s->_ref--;
  return;
}

int _API str_compare(str_t* s, const char* to) {
  if (!s || !to || BAD_MAGIC(s->_magic, TF_STR_MAGIC)) {
    set_errno(EINVAL);
    return -1;
  }
  return strncmp(s->str, to, TURF_MAX_STR_LEN);
}

// api for free char**
void _API ssfree(char** ss) {
  char** p = ss;
  if (!p) {
    return;
  }

  while (*p) {
    free(*p);
    p++;
  }

  free(ss);
}

char _API** sscopy(char** ss) {
  int i;
  char** p = ss;
  int size = 0;
  while (*p++) {
    size++;
  }

  char** n = (char**)calloc(size + 1, sizeof(char*));
  if (!n) {
    set_errno(ENOMEM);
    return NULL;
  }

  for (i = 0; i < size; i++) {
    n[i] = strdup(ss[i]);
  }

  return n;
}

int _API sssize(char** ss) {
  int i = 0;
  if (!ss) return 0;
  while (*ss++) i++;
  return i;
}

tf_arg _API* arg_new(size_t init_size) {
  int rc;
  tf_arg* arg = (tf_arg*)calloc(1, sizeof(tf_arg));
  if (!arg) {
    set_errno(ENOMEM);
    return NULL;
  }

  arg->_magic = TF_ARG_MAGIC;

  if (init_size > 0) {
    rc = arg_resize(arg, init_size);
    if (rc < 0) {
      arg_free(arg);
      return NULL;
    }
  }

  return arg;
}

int _API arg_resize(tf_arg* arg, size_t new_size) {
  if (!arg || BAD_MAGIC(arg->_magic, TF_ARG_MAGIC) ||
      (new_size < arg->_setsize)) {
    set_errno(EINVAL);
    return -1;
  }

  if (new_size == arg->_setsize) {
    return 0;
  }

  char** val = (char**)calloc(new_size + 1, sizeof(char*));
  if (!val) {
    set_errno(ENOMEM);
    return -1;
  }

  if (arg->argc > 0) {
    memcpy(val, arg->argv, arg->argc * sizeof(char*));
  }

  if (arg->argv) {
    free(arg->argv);
  }
  arg->argv = val;
  arg->_setsize = new_size;
  return 0;
}

int _API arg_add(tf_arg* arg, const char* val) {
  int rc;
  size_t spc;
  if (!arg || !val || BAD_MAGIC(arg->_magic, TF_ARG_MAGIC)) {
    set_errno(EINVAL);
    return -1;
  }

  // check space
  spc = arg->_setsize - arg->argc;
  if (spc < 1) {
    rc = arg_resize(arg, arg->_setsize + TF_ARG_STEP_SIZE);
    if (rc < 0) {
      return -1;
    }
  }
  assert(arg->argc < arg->_setsize);

  // store
  arg->argv[arg->argc++] = strdup(val);

  return 0;
}

tf_arg _API* arg_parse(const char* str) {
  char* buf = NULL;
  int rc = 0;
  tf_arg* arg = NULL;

  if (!str) {
    set_errno(EINVAL);
    return NULL;
  }

  size_t len = strlen(str);
  if (len <= 0) {
    return NULL;
  }

  arg = arg_new(0);
  if (!arg) {
    set_errno(ENOMEM);
    return NULL;
  }

  /* for const string may store in read-only media,
   * and strtok() needs break string by replace with '\0',
   * so we have to strcpy() the string before token.
   */
  buf = (char*)malloc(len + 1);
  if (!buf) {
    set_errno(ENOMEM);
    goto exit;
  }

  strncpy(buf, str, len + 1);
  buf[len] = 0;

  char* p = strtok(buf, " ");
  while (p) {
    rc = arg_add(arg, p);
    if (rc < 0) {
      goto exit;
    }
    p = strtok(NULL, " ");
  }

exit:
  // free temp buf
  if (buf) {
    free(buf);
  }

  // OK return
  if (rc == 0) {
    return arg;
  }

  // error return
  if (arg) {
    arg_free(arg);
  }
  return NULL;
}

void _API arg_free(tf_arg* arg) {
  if (!arg || BAD_MAGIC(arg->_magic, TF_ARG_MAGIC)) {
    set_errno(EINVAL);
    return;
  }

  if (arg->argv) {
    ssfree(arg->argv);
    arg->argv = NULL;
  }

  arg->_magic = 0;
  free(arg);
}

// new environ
tf_env _API* env_new(size_t init_size) {
  int rc;
  tf_env* env = (tf_env*)calloc(1, sizeof(tf_env));
  if (!env) {
    set_errno(ENOMEM);
    return NULL;
  }

  env->_magic = TF_ENV_MAGIC;

  if (init_size > 0) {
    rc = env_resize(env, init_size);
    if (rc < 0) {
      env_free(env);
      return NULL;
    }
  }

  return env;
}

// resize a environ
int _API env_resize(tf_env* env, size_t new_size) {
  if (!env || BAD_MAGIC(env->_magic, TF_ENV_MAGIC) || (new_size < env->size)) {
    set_errno(EINVAL);
    return -1;
  }

  if (new_size == env->size) {
    return 0;
  }

  char** n = (char**)calloc(new_size + 1, sizeof(char*));
  if (!n) {
    set_errno(ENOMEM);
    return -1;
  }

  if (env->size > 0) {
    memcpy(n, env->environ, (env->size * sizeof(char*)));
  }

  if (env->environ) {
    free(env->environ);
  }
  env->environ = n;
  env->_setsize = new_size;

  return 0;
}

void _API env_free(tf_env* env) {
  if (!env || BAD_MAGIC(env->_magic, TF_ENV_MAGIC)) {
    set_errno(EINVAL);
    return;
  }

  if (env->environ) {
    ssfree(env->environ);
    env->environ = NULL;
  }

  env->_magic = 0;
  free(env);
}

int _API env_set(tf_env* env, const char* name, const char* value) {
  int rc;
  size_t i, spc;
  char *c, *n;

  if (!env || !name || !value || BAD_MAGIC(env->_magic, TF_ENV_MAGIC)) {
    set_errno(EINVAL);
    return -1;
  }

  for (i = 0; i < env->size; i++) {
    c = env->environ[i];

    // reaches end
    if (!c) {
      break;
    }

    // found
    if (strncmp(c, name, strlen(name)) == 0) {  // found
      rc = xasprintf(&n, "%s=%s", name, value);
      if (rc < 0) {
        return -1;
      }

      env->environ[i] = n;
      free(c);
      return 0;
    }
  }

  // check space
  spc = env->_setsize - env->size;
  if (spc < 1) {
    rc = env_resize(env, env->_setsize + TF_ENV_STEP_SIZE);
    if (rc < 0) {
      return -1;
    }
  }
  assert(env->size < env->_setsize);

  rc = xasprintf(&c, "%s=%s", name, value);
  if (rc < 0) {
    return -1;
  }
  env->environ[env->size++] = c;
  return 0;
}

int _API env_add(tf_env* env, const char* name_value) {
  int rc = -1;
  size_t spc = 0;

  if (!env || !name_value || BAD_MAGIC(env->_magic, TF_ENV_MAGIC)) {
    set_errno(EINVAL);
    return -1;
  }

  // check space
  spc = env->_setsize - env->size;
  if (spc < 1) {
    rc = env_resize(env, env->_setsize + TF_ENV_STEP_SIZE);
    if (rc < 0) {
      return -1;
    }
  }
  assert(env->size < env->_setsize);

  env->environ[env->size++] = strdup(name_value);
  return 0;
}

int _API env_get(tf_env* env, const char* name, const char** value) {
  size_t i;
  char* c;

  if (!env || !name || !value || BAD_MAGIC(env->_magic, TF_ENV_MAGIC)) {
    set_errno(EINVAL);
    return -1;
  }
  // dprint("%x,%d,%d", env->magic, env->size, env->total);

  for (i = 0; i < env->size; i++) {
    c = env->environ[i];

    // reaches end
    if (!c) {
      break;
    }

    // found
    if (strncmp(c, name, strlen(name)) == 0) {  // found
      *value = c + strlen(name) + 1;
      return 0;
    }
  }

  set_errno(ENOENT);
  return -1;
}
