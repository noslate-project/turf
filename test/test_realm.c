#include "bdd-for-c.h"
#include "realm.h"

spec("turf.realm.intf") {
  it("test cfg") {}

  it("rlm.state") {
    struct {
      uint32_t v;
      const char* s;
    } array[] = {
        {RLM_STATE_INIT, "init"},
        {RLM_STATE_STARTING, "starting"},
        {RLM_STATE_RUNNING, "running"},
        {RLM_STATE_STOPPING, "stopping"},
        {RLM_STATE_STOPPED, "stopped"},
    };

    int i;
    for (i = 0; i < ARRAY_SIZE(array); i++) {
      struct rlm_t r = {0};
      r.st.state = array[i].v;
      check(strcmp(array[i].s, rlm_state_str(r.st.state)) == 0);
      check(rlm_state(array[i].s) == array[i].v);
    }
  }
}
