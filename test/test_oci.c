#include "bdd-for-c.h"
#include "oci.h"
#include "realm.h"
#include "spec.h"

spec("turf.oci") {
  it("oci.spec.json") {
    int rc;
    struct oci_spec* cfg1 = NULL;
    struct oci_spec* cfg2 = NULL;
    struct oci_spec* cfg3 = NULL;
    char* json1 = NULL;
    char* json2 = NULL;

    // load 1st cfg from default spec
    cfg1 = oci_spec_loads(get_spec_json());
    check(cfg1 != NULL);

    // saves 1st time
    rc = oci_spec_saves(cfg1, &json1);
    check(rc == 0);
    check(strlen(json1) > 0);
    puts(json1);

    // load 2nc cfg from json1 saved
    cfg2 = oci_spec_loads(json1);
    check(cfg2 != NULL);

    // saves 2nd time
    rc = oci_spec_saves(cfg2, &json2);
    check(rc == 0);
    check(strlen(json2) > 0);

    // load 3rd cfg from json2
    cfg3 = oci_spec_loads(json2);
    check(cfg3 != NULL);

    // the json generate twice should be same.
    check(strcmp(json1, json2) == 0);

    // free all resources
    oci_spec_free(cfg1);
    oci_spec_free(cfg2);
    oci_spec_free(cfg3);
    free(json1);
    free(json2);
  }

  it("oci.spec.loadsave") {
    int rc;
    char* file_name;
    xasprintf(&file_name, "/tmp/test_oci.%d.json", getpid());
    struct oci_spec* cfg1 = NULL;
    struct oci_spec* cfg2 = NULL;

    // load default json
    cfg1 = oci_spec_loads(get_spec_json());

    // save to temp file
    rc = oci_spec_save(cfg1, file_name);
    check(rc == 0);

    // load temp file, should decode fine
    cfg2 = oci_spec_load(file_name);
    check(cfg2 != NULL);

    // check some data
    check(cfg1->linuxs.resources.mem_limit == cfg2->linuxs.resources.mem_limit);
    check(strlen(cfg2->turf.runtime) > 0);
    check(strcmp(cfg1->turf.runtime, cfg2->turf.runtime) == 0);

    // free
    oci_spec_free(cfg1);
    oci_spec_free(cfg2);
    unlink(file_name);
    free(file_name);
  }

  it("oci.state") {
    int rc;
    char* file_name;
    xasprintf(&file_name, "/tmp/test_oci.%d.json", getpid());

    // craete a test state
    struct oci_state* state =
        oci_state_create("oci.state.test", "/tmp/oci.state.test.dir/");
    check(state != NULL);

    // makeup
    struct timeval r_now;
    get_current_time(&r_now);
    const pid_t r_pid = 555;
    const int r_state = RLM_STATE_STOPPED;

    state->pid = r_pid;
    state->status = r_state;
    state->created = r_now;
    state->stopped = r_now;

    // save it
    rc = oci_state_save(state, file_name);
    check(rc == 0);

    // load and inspect
    struct oci_state* state2 = oci_state_load(file_name);
    check(state2 != NULL);
    check(strcmp(state2->id, "oci.state.test") == 0);
    check(strcmp(state2->bundle, "/tmp/oci.state.test.dir/") == 0);
    check(state2->pid == r_pid);
    check(state2->status == r_state);
    check(memcmp(&state2->created, &r_now, sizeof(r_now)) == 0);
    check(memcmp(&state2->stopped, &r_now, sizeof(r_now)) == 0);

    // free resrc
    oci_state_free(state);
    oci_state_free(state2);
    unlink(file_name);
    free(file_name);
  }

  it("oci.blank_args") {
    struct oci_spec* cfg1 = oci_spec_load("./test/bad_spec_blank.json");
    check(cfg1 != NULL);
  }

  it("oci.seed") {
    int rc;
    char* file_name;
    xasprintf(&file_name, "/tmp/test_oci.%d.json", getpid());
    struct oci_spec* cfg1 = NULL;
    struct oci_spec* cfg2 = NULL;

    // load default json
    cfg1 = oci_spec_loads(get_spec_json());

    cfg1->turf.is_seed = true;
    cfg1->turf.has.is_seed = 1;

    // save to temp file
    rc = oci_spec_save(cfg1, file_name);
    check(rc == 0);

    // load temp file, should decode fine
    cfg2 = oci_spec_load(file_name);
    check(cfg2 != NULL);

    // check some data
    check(cfg1->turf.is_seed == cfg2->turf.is_seed);
    check(cfg1->turf.has.is_seed);
    check(cfg2->turf.has.is_seed);

    // free
    oci_spec_free(cfg1);
    oci_spec_free(cfg2);
    unlink(file_name);
    free(file_name);
  }
}
