#ifndef _TURF_MISC_H_
#define _TURF_MISC_H_

#include "inc.h"

// getconf PAGESIZE
#ifndef PAGE_SIZE
#define PAGE_SIZE 4096
#endif

#if PAGE_SIZE != 4096
#error PAGE_SIZE
#endif

// getconf CLK_TCK
#define HZ 100

// only API will be exported.
#define _API __attribute__((__visibility__("default")))

// builtin
#define LIKELY(x) __builtin_expect((x), 1)
#define UNLIKELY(x) __builtin_expect((x), 0)

// useful macros
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

// max and min macro
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

// define struct magic
#define DEF_MAGIC(a, b, c, d) (d << 24 | c << 16 | b << 8 | a)
#define BAD_MAGIC(x, y) ((x) != (y))

// errno
#define set_errno(val) (errno = (val))

#define UNUSED(x) (x = x)

// mem alignment
#define MEM_ALIGN(size, by) (((size) + by - 1) & ~(by - 1))

// logging
#ifdef USE_SYSLOG  // syslog doesn't work in docker.
#include <syslog.h>
#define logfmt(x, fmt, ...) syslog(x, "turf[%d]: " fmt, getpid(), ##__VA_ARGS__)
#else
/* all turf log outputs to stderr,
 * stdout is used for turf state info ouput.
 * example,
 * turf[1314] D: message info.
 */

#define LOG_ERROR 'E'
#define LOG_WARNING 'W'
#define LOG_INFO 'I'
#define LOG_DEBUG 'D'

#define logfmt(x, fmt, ...)                                                    \
  {                                                                            \
    time_t __t = time(NULL);                                                   \
    struct tm __tm;                                                            \
    localtime_r(&__t, &__tm);                                                  \
    struct timespec __ts;                                                      \
    clock_gettime(CLOCK_REALTIME, &__ts);                                      \
    fprintf(stderr,                                                            \
            "turf[%d] %c %d-%02d-%02d %02d:%02d:%02d:%03ld : " fmt,            \
            getpid(),                                                          \
            x,                                                                 \
            __tm.tm_year + 1900,                                               \
            __tm.tm_mon + 1,                                                   \
            __tm.tm_mday,                                                      \
            __tm.tm_hour,                                                      \
            __tm.tm_min,                                                       \
            __tm.tm_sec,                                                       \
            __ts.tv_nsec / 1000000,                                            \
            ##__VA_ARGS__);                                                    \
  }
#endif

#ifdef USE_DIE_ABORT  // prevent generate coredump.
#define _DIE() abort()
#else
#define _DIE() exit(6)
#endif

#define fatal(fmt, ...)                                                        \
  do {                                                                         \
    logfmt(LOG_ERROR, fmt, ##__VA_ARGS__);                                     \
    _DIE();                                                                    \
  } while (0)

#define die(fmt, ...)                                                          \
  fatal("%s:%d " fmt ": %m\n", __func__, __LINE__, ##__VA_ARGS__)
#define error(fmt, ...) logfmt(LOG_ERROR, fmt "\n", ##__VA_ARGS__)
#define warn(fmt, ...) logfmt(LOG_WARNING, fmt "\n", ##__VA_ARGS__)
#define pwarn(fmt, ...) logfmt(LOG_WARNING, fmt ": %m\n", ##__VA_ARGS__)
#define info(fmt, ...) logfmt(LOG_INFO, fmt "\n", ##__VA_ARGS__)
#define die_oom() die("Out of memory")

#define dprint(fmt, ...)                                                       \
  if (turf_debug_enabled()) {                                                  \
    logfmt(LOG_DEBUG, fmt "\n", ##__VA_ARGS__);                                \
  }
void _hexbuf(const char*, int);
#define hexbuf(...)                                                            \
  if (turf_debug_enabled()) {                                                  \
    _hexbuf(__VA_ARGS__);                                                      \
  }

// TURF constants, LEN includes tail '\0'.
// The max length of a string size turf can handle.
#define TURF_MAX_STR_LEN (128 * 1024)

// The max file size turf can handle.
#define TURF_MAX_FILE_SIZE (4 * 1024 * 1024)

// The max path length turf can handle.
#define TURF_MAX_PATH_LEN (1024)

// the max command line length turf can handle.
#define TURF_MAX_CMD_LEN (2048)

// the minium lentgh of a sandbox name should have.
#define TURF_SBX_MIN_LEN (4)

// the maxium length of a sandbox name can have.
#define TURF_SBX_MAX_LEN (254)

#define TF_STR_MAGIC DEF_MAGIC('S', 'T', 'R', 'T')

bool turf_debug_enabled();

// string object
struct str_t {
  uint32_t _magic;  // TF_STR_MAGIC
  int _ref;         // reference count
  char* str;        // point to string storage
};
typedef struct str_t str_t;

// create a new copy of *s
str_t _API* str_new(const char* s);

// add referenct to str_t obj
str_t _API* str_copy(str_t* from);

// dereferece or last free
void _API str_free(str_t* s);

// same as strcmp()
int _API str_compare(str_t* s, const char* to);

// free char**
char _API** sscopy(char** ss);

// free char**
void _API ssfree(char** ss);

// return size of char**
int _API sssize(char** ss);

#define TF_ARG_MAGIC DEF_MAGIC('T', 'A', 'R', 'G')

// auto resize +size
#define TF_ARG_STEP_SIZE (10)

// arg obj
struct tf_arg {
  uint32_t _magic;  // TF_ARG_MAGIC
  size_t _setsize;  // total size includes NULL ptr.
  size_t argc;      // arguments count
  char** argv;      // pointer to argv (char**)
};
typedef struct tf_arg tf_arg;

// create a blank arg obj
tf_arg _API* arg_new(size_t init_size);

// resize arg storage
int _API arg_resize(tf_arg* arg, size_t new_size);

// add val to arg
int _API arg_add(tf_arg* arg, const char* val);

// parse from command line
tf_arg _API* arg_parse(const char* str);

// free arg obj
void _API arg_free(tf_arg* arg);

// MAGIC value for tf_env
#define TF_ENV_MAGIC DEF_MAGIC('T', 'E', 'N', 'V')

// auto resize +size
#define TF_ENV_STEP_SIZE (10)

// environ obj
struct tf_env {
  uint32_t _magic;  // TF_ENV_MAGIC
  size_t _setsize;  // total size
  size_t size;      // current size, without last NULL ptr.
  char** environ;   // raw char **
};
typedef struct tf_env tf_env;

// create a new env
tf_env _API* env_new(size_t init_size);

// create a new env
int _API env_resize(tf_env* env, size_t new_size);

// update env with K and V
int _API env_set(tf_env* env, const char* name, const char* value);

// add item to environ
int _API env_add(tf_env* env, const char* name_value);

// get value by name
int _API env_get(tf_env* env, const char* name, const char** value);

// free tf_env memory
void _API env_free(tf_env* env);

/* misc api
 */

// format string to &str, should free() after use.
int _API xasprintf(char** str, const char* fmt, ...);

// read file
int _API read_file(const char* path, char** pbuf, size_t* psize);

// write file
int _API write_file(const char* path, const char* buf, size_t size);

// get pid max, based on proc
int _API proc_pid_max(int* pid_max);

// get current time
int _API get_current_time(struct timeval* tv);

// timeval to ms
int64_t _API timeval2ms(const struct timeval* tv);

// format tv to string
char _API* timeval2str(struct timeval* tv);

// string to timeval
int _API str2timeval(const char* str, struct timeval* tv);

// ms to timeval
int _API ms2timeval(int64_t ms, struct timeval* tv);

// dump rusage struct, debug
void _API rusage_dump(const struct rusage* ru);

#endif  // _TURF_MISC_H_
