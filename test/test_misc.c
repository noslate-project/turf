#include "bdd-for-c.h"
#include "misc.h"

spec("turf.misc") {
  int rc;

  it("misc.magic") {
    uint32_t m1 = DEF_MAGIC('T', 'U', 'R', 'F');
    check(strncmp((char*)&m1, "TURF", 4) == 0);
    check(BAD_MAGIC(m1, TF_STR_MAGIC));
  }

  it("misc.xasprintf") {
    char* str;
    int rc = xasprintf(&str, "%s,%s,%d", "hello", "world", 1);
    check(rc == 13);
    check(str);
    check(strcmp(str, "hello,world,1") == 0);
    free(str);
  }

  it("misc.timeval") {
    struct timeval tv = {0};
    struct timeval tv2 = {0};
    uint64_t v1, v2;
    int rc;

    // read to tv
    rc = get_current_time(&tv);
    check(rc == 0);
    check(tv.tv_sec > 0);

    // zero tv to string
    const char* s1 = timeval2str(&tv2);
    check(s1);
    printf("s1: %s\n", s1);
    check(strcmp(s1, "1970-01-01T00:00:00.000000000Z") == 0);
    free(s1);

    // now to string
    const char* s2 = timeval2str(&tv);
    check(s2);
    printf("s2: %s\n", s2);
    free(s2);

    // read to tv2
    rc = get_current_time(&tv2);
    check(rc == 0);

    // convert to ms
    v1 = timeval2ms(&tv);
    v2 = timeval2ms(&tv2);
    check(v1 <= v2);
  }

  it("str2timeval") {
    // get now
    struct timeval now;
    rc = get_current_time(&now);
    check(rc == 0);

    // to string s1
    const char* s1 = timeval2str(&now);
    check(s1 != NULL);
    printf("s1: %s\n", s1);

    // back to tv1, should equal to now
    struct timeval t1;
    rc = str2timeval(s1, &t1);
    check(rc == 0);
    check(t1.tv_sec == now.tv_sec);
    check(t1.tv_usec == now.tv_usec);

    // to string s2, should equal to s1
    const char* s2 = timeval2str(&t1);
    check(s2 != NULL);

    printf("s2: %s\n", s2);
    check(strcmp(s1, s2) == 0);

    // free resources
    free(s1);
    free(s2);
  }

  it("str_t") {
    str_t* a = str_new("helloworld");
    check(a->_ref == 1);
    check(a->str != 0);
    int rc = str_compare(a, "helloworld");
    check(rc == 0);

    str_t* b = str_copy(a);
    check(a == b);
    check(b->_ref == 2);
    rc = str_compare(b, "helloworld");
    check(rc == 0);

    str_t* c = str_copy(b);
    check(c == b);
    check(c->_ref == 3);
    rc = str_compare(c, "helloworld");
    check(rc == 0);

    str_free(b);
    check(a->_ref == 2);
    rc = str_compare(a, "helloworld");
    check(rc == 0);
    rc = str_compare(c, "helloworld");
    check(rc == 0);

    str_free(a);
    check(c->_ref == 1);
    rc = str_compare(c, "helloworld");
    check(rc == 0);

    str_free(c);
    // check(c->_ref == 0);
  }

  it("ssfree") {
    char** av = calloc(10, sizeof(char*));
    av[0] = strdup("apple");
    av[1] = strdup("banana");
    av[2] = strdup("orange");

    char** bv = sscopy(av);
    check(strcmp(bv[0], av[0]) == 0);
    check(strcmp(bv[1], av[1]) == 0);
    check(strcmp(bv[2], av[2]) == 0);

    ssfree(av);
    ssfree(bv);
  }

  it("arguments.parse") {
    tf_arg* arg = arg_new(0);
    check(arg);
    arg_add(arg, "turf");
    arg_add(arg, "start");
    arg_add(arg, "sandbox_name");
    check(arg->argc == 3);

    tf_arg* arg2 = arg_parse("turf start sandbox_name");
    check(arg2);
    check(arg2->argc == 3);

    check(strcmp(arg->argv[0], arg2->argv[0]) == 0);
    check(strcmp(arg->argv[1], arg2->argv[1]) == 0);
    check(strcmp(arg->argv[2], arg2->argv[2]) == 0);

    arg_free(arg);
    arg_free(arg2);
  }

  it("environ.new_free") {
    tf_env* env;

    env = env_new(0);
    check(env);
    check(env->_magic == TF_ENV_MAGIC);
    check(env->_setsize == 0);
    check(env->size == 0);
    check(env->environ == NULL);
    env_free(env);

    env = env_new(9);
    check(env);
    check(env->_magic == TF_ENV_MAGIC);
    check(env->_setsize == 9);
    check(env->size == 0);
    check(env->environ);
    env_free(env);
  }

  it("environ.resize") {
    tf_env* env;

    // auto resize from 0
    env = env_new(0);
    check(env->environ == NULL);
    rc = env_add(env, "A=a");
    check(rc == 0);
    check(env->_setsize == TF_ENV_STEP_SIZE);
    check(env->environ);
    env_free(env);

    // auto resize full
    env = env_new(1);
    check(env->_setsize == 1);
    rc = env_add(env, "A=a");
    check(rc == 0);
    check(env->_setsize == 1);
    check(env->size == 1);
    rc = env_add(env, "B=b");
    check(rc == 0);
    check(env->_setsize == TF_ENV_STEP_SIZE + 1);
    check(env->size == 2);
    env_free(env);

    // manual resize
    env = env_new(1);
    check(env->_setsize == 1);
    rc = env_add(env, "A=a");
    check(rc == 0);
    rc = env_resize(env, 2);
    check(rc == 0);
    check(env->_setsize == 2);
    check(env->size == 1);
    rc = env_add(env, "B=b");
    check(rc == 0);
    check(env->_setsize == 2);
    check(env->size == 2);
    env_free(env);
  }

  it("environ.set_get_add") {
    char* path = "/usr/bin:/usr/sbin:/bin:/sbin";
    char* abc = "abcdefghijklmopqrstuvwxyz";
    char* v;
    tf_env* env = env_new(0);
    rc = env_set(env, "PATH", path);
    check(rc == 0);
    rc = env_set(env, "ABC", abc);
    check(rc == 0);
    rc = env_add(env, "CBS=");
    check(rc == 0);
    rc = env_get(env, "PATH", &v);
    check(rc == 0);
    check(strcmp(v, path) == 0);
    rc = env_get(env, "ABC", &v);
    check(rc == 0);
    check(strcmp(v, abc) == 0);
    rc = env_get(env, "CBS", &v);
    check(rc == 0);
    check(strcmp(v, "") == 0);
    env_free(env);
  }
}
